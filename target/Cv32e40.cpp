// CV32E40 ITarget interface implementation
//
// This file is part of the Embecosm Debug Server targets.
//
// Copyright (C) 2020 Embecosm Limited
// SPDX-License-Identifier: MIT
// ----------------------------------------------------------------------------

#include "Cv32e40.h"
#include "embdebug/Compat.h"
#include "embdebug/ITarget.h"

#include <iostream>
#include <memory>

using namespace EmbDebug;

using namespace std;

#define CV32E40_TARGET_DEBUG 1

/* This bit mask corresponds to the step field of the dcsr register. */
#define MASK_DCSR_STEP (1 << 2)

/* This bit mask corresponds to the ebreakm, ebreaks, and ebreaku fields of the
 * dcsr register */
#define MASK_DCSR_EBREAK_FIELDS ((1 << 15) | (1 << 13) | (1 << 12))

/* This bit mask corresponds to the cause field of the dcsr register. */
#define MASK_DCSR_CAUSE_FIELD 0x000001C0

/* This is the bit index where the cause field of the dcsr register begins. */
#define DCSR_CAUSE_FIELD_START 6

/* This is the value the cause field will be set to if an ebreak instruction was
 * executed. */
#define DCSR_CAUSE_EBREAK_EXECUTED 1

/* Each register has its own unique number identifier, this is the identifier
 * for PC */
#define REG_PC_IDENTIFIER 0x20

/* This bit mask corresponds to the field for the first hart in haltsum. */
#define MASK_HALTSUM_FIRST_HART 1

// Instantiate the model. TODO the argument will change to pass in the
// residual argv.
Cv32e40::Cv32e40 (const TraceFlags *traceFlags) : ITarget (traceFlags)
{
  // We create the DTM here, because only at this level do we know what
  // derived class we will instantiate.  But we then pass ownership to the
  // DMI, because that is where it belongs.
  unique_ptr<IDtm> mDtm (new DtmJtag (20, 1000000000, ""));
  unique_ptr<Dmi> mDmi (new Dmi (std::move (mDtm)));
  // Only one core is present so we can select it at the start
  mDmi->dtmReset ();
  mDmi->selectHart (0);
  mDmi->haltHart (0);
  mDmi->dmcontrol ()->read ();
  mDmi->dmcontrol ()->prettyPrint (1);
  mDmi->dmstatus ()->prettyPrint (1);

  mDmi->dmstatus ()->read ();

  // Get sim start time
  const uint64_t simStart = mDmi->simTimeNs ();

  // Move from method local storage to object attributes
  this->mDmi = std::move (mDmi);
  this->simStart = simStart;
  return;
}

// Clean up the model
Cv32e40::~Cv32e40 ()
{
  mDmi.reset (nullptr);
  return;
}

// Force termination of the model execution
ITarget::ResumeRes
Cv32e40::terminate ()
{
  return ITarget::ResumeRes::NONE;
}

// Reset the model state
ITarget::ResumeRes Cv32e40::reset (ITarget::ResetType)
{
  mDmi.reset ();
  return ITarget::ResumeRes::SUCCESS;
}

// Count cycles
uint64_t
Cv32e40::getCycleCount () const
{
  return mCycleCnt;
}

// Count instructions
uint64_t
Cv32e40::getInstrCount () const
{
  return mInstrCnt;
}

// How many registers do we have?
int
Cv32e40::getRegisterCount () const
{
  return 32;
}

// How big is each register in bytes
int
Cv32e40::getRegisterSize () const
{
  return 4;
}

// Read a register into the VALUE reference argument, returning the number of
// bytes read
std::size_t
Cv32e40::readRegister (const int reg, uint_reg_t &value)
{
  std::size_t retval = getRegisterSize ();

  uint32_t readvalue; // Need a temp store since we can't pass value.
  if (reg == REG_PC_IDENTIFIER)
    {
      mDmi->readCsr (Dmi::Csr::DPC, readvalue);
    }
  else
    {
      mDmi->readGpr (reg, readvalue);
    }
  value = readvalue;

  return retval;
}

// This is the mechanism for handling host file I/O. It provides a mechanism
// for supplying arguments and returning results. In this template we report
// that the syscall number is in register 1, the arguments in registers 2-4
// and the result in register 5. Consult your ABI to determine the correct
// values for a real architecture. Return if this was successful or not.
bool
Cv32e40::getSyscallArgLocs (SyscallArgLoc &syscallIDLoc,
                            std::vector<SyscallArgLoc> &syscallArgLocs,
                            SyscallArgLoc &syscallReturnLoc) const
{
  syscallIDLoc
      = SyscallArgLoc::RegisterLoc ({ SyscallArgLocType::REGISTER, 1 });

  syscallArgLocs.clear ();
  syscallArgLocs.push_back (
      SyscallArgLoc::RegisterLoc ({ SyscallArgLocType::REGISTER, 2 }));
  syscallArgLocs.push_back (
      SyscallArgLoc::RegisterLoc ({ SyscallArgLocType::REGISTER, 3 }));
  syscallArgLocs.push_back (
      SyscallArgLoc::RegisterLoc ({ SyscallArgLocType::REGISTER, 4 }));

  // Return value in a0
  syscallReturnLoc
      = SyscallArgLoc::RegisterLoc ({ SyscallArgLocType::REGISTER, 5 });
  return true;
}

// Write a register supplied in the VALUE argument and returning the number of
// bytes written
std::size_t
Cv32e40::writeRegister (const int reg, const uint_reg_t value)
{
  std::size_t retval = getRegisterSize ();
  if (reg == REG_PC_IDENTIFIER)
    { // Special case for pc as it is a CSR not GPR
      mDmi->writeCsr (Dmi::Csr::DPC, value);
    }
  else
    {
      mDmi->writeGpr (reg, value);
    }
  return retval;
}

// Read a block of memory into the supplied buffer, returning the number of
// bytes read
std::size_t
Cv32e40::read (const uint_addr_t addr, uint8_t *buffer, const std::size_t size)
{
  // We use an intermediary buffer 'buf' before loading into the given buffer.
  std::size_t retval = size;
  std::unique_ptr<unsigned char[]> buf (new unsigned char[size]);
  mDmi->readMem (addr, size, buf);
  for (size_t i = 0; i < size; i++)
    buffer[i] = buf[i];
  return retval;
  ;
}

// Write a block of memory from the supplied buffer, returning the number of
// bytes read
std::size_t
Cv32e40::write (const uint_addr_t addr, const uint8_t *buffer,
                const std::size_t size)
{
  // We can be passed zero, just as a test that the X packet is supported.
  if (size == 0)
    return size;

  // We use an intermediary buffer 'buf' before loading into the given buffer.
  std::size_t retval = size;
  std::unique_ptr<unsigned char[]> buf (new unsigned char[size]);
  for (size_t i = 0; i < size; i++)
    buf[i] = buffer[i];
  mDmi->writeMem (addr, size, buf);
  return retval;
}

// Insert a matchpoint (breakpoint or watchpoint), returning whether or not
// this succeeded
bool
Cv32e40::insertMatchpoint (const uint_addr_t addr, const MatchType matchType)
{
  return false;
}

// Delete a matchpoint (breakpoint or watchpoint), returning whether or not
// this succeeded
bool
Cv32e40::removeMatchpoint (const uint_addr_t addr, const MatchType matchType)
{
  return false;
}

// Passthru' a command to the target, returning whether or not this succeeded.
bool
Cv32e40::command (const std::string cmd, std::ostream &stream)
{
  return false;
}

// Return the time taken by the CPU so far in seconds
double
Cv32e40::timeStamp ()
{
  return mCpuTime;
}

// Return the number of CPUs
unsigned int
Cv32e40::getCpuCount (void)
{
  return 1; // Only one CPU is available
}

// Return the curent CPU (must be consistent with the number of CPUs, use -1
// to indicate an invalid response
unsigned int
Cv32e40::getCurrentCpu (void)
{
  return 0; // Only one CPU is available
}

// Specify the current CPU
void
Cv32e40::setCurrentCpu (unsigned int num)
{
  assert (num == 0); // Only one CPU is available
  return;
}

// Prepare each core to be resumed. The supplied vector, ACTIONS, says what
// each core should do when resume is next called. Return whether this
// succeeded.
bool
Cv32e40::prepare (const std::vector<ITarget::ResumeType> &actions)
{
  bool retval = false;
  if (actions.size () == 1)
    {
      mRunAction = actions.at (0);
      switch (mRunAction)
        {

        case ITarget::ResumeType::STEP:
          retval = true;
          break;
        case ITarget::ResumeType::CONTINUE:
          retval = true;
          break;
        case ITarget::ResumeType::NONE:
          break;
        default:
          break;
        }
    }
  return retval;
}

// Resume each core, according to what was specified in the earlier call to
// prepare. Return whether this succeeded.
bool
Cv32e40::resume (void)
{
  assert (
      mRunAction
      != ITarget::ResumeType::NONE); // NONE is invalid on single core machine
  bool retval = true;

  // Explicitly disable halt request and enable resume request.
  mDmi->dmcontrol ()->haltreq (false);

  if (mRunAction == ITarget::ResumeType::STEP)
    {
      // Enable step field of dcsr register.
      uint32_t dbg_ctrl_val;
      retval &= mDmi->readCsr (Dmi::Csr::DCSR, dbg_ctrl_val)
                == Dmi::Abstractcs::CmderrVal::CMDERR_NONE;
      dbg_ctrl_val |= MASK_DCSR_STEP;
      retval &= mDmi->writeCsr (Dmi::Csr::DCSR, dbg_ctrl_val)
                == Dmi::Abstractcs::CmderrVal::CMDERR_NONE;
    }
  else if (mRunAction == ITarget::ResumeType::CONTINUE)
    {
      // Enable ebreak fields in dcsr register.
      uint32_t dbg_ctrl_val;
      retval &= mDmi->readCsr (Dmi::Csr::DCSR, dbg_ctrl_val)
                == Dmi::Abstractcs::CmderrVal::CMDERR_NONE;
      dbg_ctrl_val |= MASK_DCSR_EBREAK_FIELDS;
      retval &= mDmi->writeCsr (Dmi::Csr::DCSR, dbg_ctrl_val)
                == Dmi::Abstractcs::CmderrVal::CMDERR_NONE;
    }
  mDmi->dmcontrol ()->resumereq ();
  mDmi->dmcontrol ()->write ();

  return retval;
}

// Clock the model waiting for any core to stop
ITarget::WaitRes
Cv32e40::wait (std::vector<ResumeRes> &results)
{

  assert (mRunAction != ITarget::ResumeType::NONE);
  ITarget::WaitRes retval = ITarget::WaitRes::ERROR;

  results.clear (); // Make sure the space for our results is cleared.
  results.resize (getCpuCount ()); // We want to return one result per CPU.

  switch (mRunAction)
    {

    case ITarget::ResumeType::STEP:
      retval = stepInstr (results[0]);
      break;

    case ITarget::ResumeType::CONTINUE:
      retval = runToBreak (results[0]);
      break;

    default:
      std::cerr << "*** ABORT ***: Unknown step type when resuming: "
                << mRunAction << std::endl;
      retval = ITarget::WaitRes::ERROR;
      break;
    }

  return retval;
}

// Force all cores to halt, returning whether this was successful
bool
Cv32e40::halt (void)
{
  bool retval = true;

  mDmi->haltHart (0);
  mDmstatus->read ();
  if (!mDmstatus->halted ())
    {
      mDmstatus->prettyPrint (false);
      mDmstatus->prettyPrint (true);
      retval = false;
    }

  return retval;
}

ITarget::WaitRes
Cv32e40::stepInstr (ITarget::ResumeRes &resumeRes)
{
  ITarget::WaitRes retval = ITarget::WaitRes::EVENT_OCCURRED;
  /* Keep going until we halt */
  uint32_t haltsum_val;
  while (1)
    {
      mDmi->haltsum ()->read (0);
      haltsum_val = mDmi->haltsum ()->haltsum (0);
      /* If hart 1 is halted, break */
      if (haltsum_val & MASK_HALTSUM_FIRST_HART)
        {
          resumeRes = ITarget::ResumeRes::INTERRUPTED;
          break;
        }
    }
  /* Unset the step field. */
  uint32_t dcsr_val;
  mDmi->readCsr (Dmi::Csr::DCSR, dcsr_val);
  dcsr_val &= ~MASK_DCSR_STEP;
  mDmi->writeCsr (Dmi::Csr::DCSR, dcsr_val);

  return retval;
}

ITarget::WaitRes
Cv32e40::runToBreak (ITarget::ResumeRes &resumeRes)
{
  ITarget::WaitRes retval = ITarget::WaitRes::EVENT_OCCURRED;
  if (stoppedAtEbreak ())
    {
      resumeRes = ITarget::ResumeRes::INTERRUPTED;
    }
  else
    {
      resumeRes = ITarget::ResumeRes::FAILURE;
    }
  /* Unset the ebreak fields. */
  uint32_t dcsr_val;
  mDmi->readCsr (Dmi::Csr::DCSR, dcsr_val);
  dcsr_val &= ~MASK_DCSR_EBREAK_FIELDS;
  mDmi->writeCsr (Dmi::Csr::DCSR, dcsr_val);

  return retval;
}

bool
Cv32e40::stoppedAtEbreak ()
{
  /* Keep going until we halt */
  uint32_t haltsum_val = 0;
  while (1)
    {
      mDmi->haltsum ()->read (0);
      haltsum_val = mDmi->haltsum ()->haltsum (0);
      /* If hart 1 is halted, break */
      if (haltsum_val & MASK_HALTSUM_FIRST_HART)
        break;
    }
  /* Check if we stopped because of an ebreak */
  uint32_t dcsr_val;
  mDmi->readCsr (Dmi::Csr::DCSR, dcsr_val);
  bool stoppedAtEbreak
      = ((dcsr_val & MASK_DCSR_CAUSE_FIELD) >> DCSR_CAUSE_FIELD_START)
        == DCSR_CAUSE_EBREAK_EXECUTED;
  return stoppedAtEbreak;
}

// Entry point for the shared library
extern "C"
{
  // Create and return a new model
  EMBDEBUG_VISIBLE_API ITarget *
  create_target (TraceFlags *traceFlags)
  {
    return new Cv32e40 (traceFlags);
  }
  // Used to ensure API compatibility
  EMBDEBUG_VISIBLE_API uint64_t
  ITargetVersion ()
  {
    return ITarget::CURRENT_API_VERSION;
  }
}
