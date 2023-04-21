// Definition of a class to manage a JTAG TAP simulation.
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#include <iomanip>
#include <iostream>

#include "Tap.h"

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::setfill;
using std::setw;
using std::unique_ptr;

/// \brief Constructor for the JTAG TAP model
///
/// This instantiates an underlying Verilator model of the MCU, and all the
/// arguments are needed to create that Verilator model.
///
/// We provide a default value for the last Instruction Register used of 0,
/// which according to IEEE 1149.1 should always be BYPASS.
///
/// We don't initially know if we need to go via Run-Test/Idle when accessing
/// the same register multiple times, or how many cycles to stay there.  We
/// set this to 1 as a useful default.
///
/// \param[in] clkPeriodNs    \see VSim::VSim
/// \param[in] simTimeNs      \see VSim::VSim
/// \param[in] vcdFile        \see VSim::VSim
Tap::Tap (const uint64_t clkPeriodNs, const uint64_t simTimeNs,
          const char *vcdFile)
    : mLastIr (0), mRtiCount (1)
{
  mMcu.reset (new VSim (static_cast<const vluint64_t> (clkPeriodNs),
                        static_cast<const vluint64_t> (simTimeNs), vcdFile));
}

/// \brief Destructor for the JTAG TAP model
///
/// While the MCU should be cleaned up automatically, we do this explicitly as
/// good practice.
Tap::~Tap ()
{
  mMcu.reset (nullptr);
}

/// \brief Setter for the Run-Test/Idle count
///
/// When we access the same register multiple times, we need not set the IR
/// again, but we may need to spend one or more cycles in Run-Test/Idle.  This
/// is not known at instantiation, so we provide a mechanism to set this
/// value.
///
/// \param[in] rtiCount_  The value to save as the Run-Test/Idle count.
void
Tap::rtiCount (const uint8_t rtiCount_)
{
  mRtiCount = rtiCount_;
}

/// \brief Take the simulator through reset
///
/// This resets both the simulator and the actual JTAG TAP.
/// We also reset the JTAG TAP.
///
/// @note There is a "feature" in this particular Verilog implementation.  The
///       reset signal is optional in IEEE 1149.1.  In case it is missing, we
///       should also drive TMS high, which, so long as reset is 5 JTAG TAP
///       clock cycles, will also reset the TAP.  However a feature of this
///       implementation is that it is anticipating the next state
///       combinatorially, so this has the effect of throwing the TAP into
///       Select-DR-Scan.  We therefore must drive TMS low throughout reset.
///
/// @note There is a second "feature" of this particular Verilog
///       implementation, which is that reset places it in Run-Test/Idle, not
///       Test-Logic-Reset.
///
/// \param[in] mcu  The MCU simulator
/// \param[in] tap  The JTAG TAP simulator
/// \return \c true if we completed reset, \c false if the simulation
///         terminated during reset.
bool
Tap::reset ()
{
  while (mMcu->inReset ())
    {
      if (mMcu->allDone ())
        return false;

      mMcu->tms (0U); // Needed for this implementation
      mMcu->eval ();
      mMcu->advanceHalfPeriod ();
    }

  mCurrState = RUN_TEST_IDLE; // Should be Test-Logic-Reset
  return true;
}

/// \brief Generic access to a JTAG register.
///
/// The supplied value is written and simultaneously a register is read back.
/// We ensure we end up in Update-DR state to commit any write.
///
/// \param[in] ir  The instruction register to set.
/// \param[in] wdata  The data register to write.
/// \param[in] len    Length of data to write/read.
/// \return the value read.
uint64_t
Tap::accessReg (const uint8_t ir, uint64_t wdata, const uint8_t len)
{
  // Sanity check
  if (len > 64)
    {
      cerr << "ERROR: Attempt to access JTAG register of size "
           << static_cast<unsigned int> (len) << endl;
      exit (EXIT_FAILURE);
    }

  // If the IR has not changed we do don't need to save it, but we might need
  // to stay in Run-Test/Idle for some cycles
  if (mLastIr == ir)
    for (uint8_t i = 0; i < mRtiCount; i++)
      gotoState (RUN_TEST_IDLE);
  else
    shiftIr (ir);

  uint64_t reg = shiftDr (wdata, len);
  gotoState (UPDATE_DR);

  return reg;
}

/// \brief Generic write to a JTAG register.
///
/// Convenience wrapper of Tap::accessReg.
///
/// \param[in] ir  The instruction register to set.
/// \param[in] wdata  The data register to write.
/// \param[in] len    Length of data to write.
void
Tap::writeReg (const uint8_t ir, uint64_t wdata, const uint8_t len)
{
  static_cast<void> (accessReg (ir, wdata, len));
}

/// \brief Generic read from a JTAG register.
///
/// Convenience wrapper of Tap::accessReg.
///
/// \param[in] ir  The instruction register to set.
/// \param[in] len    Length of data to read.
/// \return the value read.
uint64_t
Tap::readReg (const uint8_t ir, const uint8_t len)
{
  return accessReg (ir, 0ULL, len);
}

/// \brief Helper to shift in an instruction register
///
/// @note The length of an instruction register is hard coded
///
/// \param[in] ireg  The instruction register to shift in.
void
Tap::shiftIr (const uint8_t ireg)
{
  static_cast<void> (gotoState (SHIFT_IR));

  // Shift in LS bit first, staying in SHIFT_IR for all but the last bit
  for (int i = 0; i < (IR_LEN - 1); i++)
    static_cast<void> (advanceState (/* tms = */ false,
                                     /* tdi = */ (ireg & (1 << i)) != 0));

  static_cast<void> (advanceState (/* tms = */ true,
                                   /* tdi = */
                                   (ireg & (1 << (IR_LEN - 1))) != 0));

  static_cast<void> (gotoState (UPDATE_IR));
}

/// \brief Provide access to simulation time
///
/// \return The current simulation time in nanoseconds.
uint64_t
Tap::simTimeNs () const
{
  return static_cast<uint64_t> (mMcu->simTimeNs ());
}

/// \brief Helper to shift a data register in and out
///
/// \param[in] dreg  The data register to shift in
/// \param[in] len   The length of the register in bits
/// \return  The register shifted out.
uint64_t
Tap::shiftDr (const uint64_t dreg, size_t len)
{
  static_cast<void> (gotoState (SHIFT_DR));

  // Shift in the first LS bit
  static_cast<void> (advanceState (/* tms = */ false,
                                   /* tdi = */ (dreg & 1) != 0));

  // Start shifting out bits
  uint64_t regOut = 0;
  for (int i = 1; i < (len - 1); i++)
    if (advanceState (/* tms = */ false,
                      /* tdi = */ (dreg & (1ULL << i)) != 0))
      regOut |= 1ULL << (i - 1);

  // Shift in the last bit and out the penultimate bit
  if (advanceState (/* tms = */ true,
                    /* tdi = */ (dreg & (1ULL << (len - 1))) != 0))
    regOut |= 1ULL << (len - 2);

  // Shift out the last bit
  if (advanceState (/* tms = */ false,
                    /* tdi = */ (dreg & (1ULL << (len - 1))) != 0))
    regOut |= 1ULL << (len - 1);

  static_cast<void> (gotoState (UPDATE_DR));
  return regOut;
}

/// \brief Helper to get to a desired state.
///
/// Drives TMS and discards all but the last TDO output.
///
/// \param[in] s    The state we are trying to reach.
/// \return \c true if the final TDO is high, \c false otherwise or on error.
bool
Tap::gotoState (const Tap::State s)
{
  // A table showing the TMS value for the first step for getting for the
  // state of the first argument, to the state of the second argument.
  static const uint8_t nextStateTab[NUM_STATES][NUM_STATES] =
      //  TLR                     UDR
      {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0 }, // Test-Logic-Reset ->
        { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Run-Test/Idle ->
        { 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }, // Select-DR-Scan ->
        { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Capture-DR ->
        { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Shift-DR ->
        { 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 }, // Exit1-DR ->
        { 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Pause-DR ->
        { 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 }, // Exit2-DR ->
        { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Update-DR ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 }, // Select-IR-Scan ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 }, // Capture-IR ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 }, // Shift-IR ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1 }, // Exit1-IR ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1 }, // Pause-IR ->
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 }, // Exit2-IR ->
        { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // Update-IR ->
      };

  // Safety check
  if (static_cast<unsigned> (s) >= NUM_STATES)
    {
      cerr << "ERROR: Attempt to go to non-existent state: " << s << endl;
      return false;
    }

  bool tdo_reg = mMcu->tdo ();

  while (mCurrState != s)
    {
      bool tms = (1U == nextStateTab[mCurrState][s]);
      // Drive TMS into TAP state machine, which will leave as at JTAG TAP
      // negative edge. We ignore the TDO output.
      tdo_reg = advanceState (tms, /* tdi = */ false);
    }

  return tdo_reg;
}

/// \brief Advance the JTAG TAP state machine
///
/// Clock the processor to a JTAG TAP positive edge, set TMS/TDI, then clock
/// the processor to a JTAG TAP negative edge, and read TDO as the result.
/// Update the model of the JTAG TAP state accordingly.
///
/// \note  The JTAG TAP is left at a negative edge.
///
/// \param[in] tms  The value to drive as the TMS input.
/// \param[in] tdi  The value to drive as the TDI input.
/// \return The resulting value of TDO.
bool
Tap::advanceState (const bool tms, const bool tdi)
{
  // Get to the JTAG TAP positive edge if we are not already there
  while (!mMcu->tapPosedge ())
    {
      mMcu->eval ();
      mMcu->advanceHalfPeriod ();
    }

  // Set TMS and drive to the next JTAG TAP negedge.
  mMcu->tms (tms);
  mMcu->tdi (tdi);

  while (!mMcu->tapNegedge ())
    {
      mMcu->eval ();
      mMcu->advanceHalfPeriod ();
    }

  // Update the record of the state we are in and then return the TDO
  nextState (mMcu->tms ());
  return mMcu->tdo ();
}

/// \brief Helper to get the next state.
///
/// Updates Tap::mCurrState with the next state, based on the TMS input
///
/// \param[in] tms  The TMS signal driving the state transition
void
Tap::nextState (const bool tms)
{
  static const State trans[NUM_STATES][2] = {
    { RUN_TEST_IDLE, TEST_LOGIC_RESET }, // Test-Logic-Reset ->
    { RUN_TEST_IDLE, SELECT_DR_SCAN },   // Run-Test/Idle ->
    { CAPTURE_DR, SELECT_IR_SCAN },      // Select-DR-Scan ->
    { SHIFT_DR, EXIT1_DR },              // Capture-DR ->
    { SHIFT_DR, EXIT1_DR },              // Shift-DR ->
    { PAUSE_DR, UPDATE_DR },             // Exit1-DR ->
    { PAUSE_DR, EXIT2_DR },              // Pause-DR ->
    { SHIFT_DR, UPDATE_DR },             // Exit2-DR ->
    { RUN_TEST_IDLE, SELECT_DR_SCAN },   // Update-DR ->
    { CAPTURE_IR, TEST_LOGIC_RESET },    // Select-IR-Scan ->
    { SHIFT_IR, EXIT1_IR },              // Capture-IR ->
    { SHIFT_IR, EXIT1_IR },              // Shift-IR ->
    { PAUSE_IR, UPDATE_IR },             // Exit1-IR ->
    { PAUSE_IR, EXIT2_IR },              // Pause-IR ->
    { SHIFT_IR, UPDATE_IR },             // Exit2-IR ->
    { RUN_TEST_IDLE, SELECT_DR_SCAN },   // Update-DR ->
  };

  mCurrState = tms ? trans[mCurrState][1] : trans[mCurrState][0];
}

/// \brief Helper to get the name of a state safely
///
/// Any value out of range is printed as its (signed) integer value in
/// decimal.
///
/// \param[in] os  The output stream
/// \param[in] s   The state to output
/// \return The updated stream
const char *
Tap::nameState (const State s) const
{
  // Array of state names
  static const char *nameTable[NUM_STATES] = {
    "Test-Logic-Reset", "Run-Test/Idle",  "Select-DR-Scan", "Capture-DR",
    "Shift-DR",         "Exit1-DR",       "Pause-DR",       "Exit2-DR",
    "Update-DR",        "Select-IR-Scan", "Capture-IR",     "Shift-IR",
    "Exit1-IR",         "Pause-IR",       "Exit2-IR",       "Update-DR",
  };

  if (static_cast<unsigned> (s) < NUM_STATES)
    return nameTable[s];
  else
    return "out-of-range";
}
