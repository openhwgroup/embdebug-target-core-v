// CV32E40 ITarget interface implementation
//
// This file is part of the Embecosm Debug Server targets.
//
// Copyright (C) 2020 Embecosm Limited
// SPDX-License-Identifier: MIT
// ----------------------------------------------------------------------------

#ifndef CV32E40_H
#define CV32E40_H

#include "Dmi.h"
#include "DtmJtag.h"
#include "embdebug/ITarget.h"

#include <map>
#include <memory>

namespace EmbDebug {

class Cv32e40 : public ITarget
{
public:
  Cv32e40 () = delete;
  Cv32e40 (const Cv32e40 &) = delete;

  explicit Cv32e40 (const TraceFlags *traceFlags);
  ~Cv32e40 ();

  ITarget::ResumeRes terminate () override;
  ITarget::ResumeRes reset (ITarget::ResetType) override;

  uint64_t getCycleCount () const override;
  uint64_t getInstrCount () const override;

  int getRegisterCount () const override;
  int getRegisterSize () const override;
  bool getSyscallArgLocs (SyscallArgLoc &syscallIDLoc,
                          std::vector<SyscallArgLoc> &syscallArgLocs,
                          SyscallArgLoc &syscallReturnLoc) const override;

  std::size_t readRegister (const int reg, uint_reg_t &value) override;
  std::size_t writeRegister (const int reg, const uint_reg_t value) override;

  std::size_t read (const uint_addr_t addr, uint8_t *buffer,
                    const std::size_t size) override;

  std::size_t write (const uint_addr_t addr, const uint8_t *buffer,
                     const std::size_t size) override;

  bool insertMatchpoint (const uint_addr_t addr,
                         const MatchType matchType) override;
  bool removeMatchpoint (const uint_addr_t addr,
                         const MatchType matchType) override;

  bool command (const std::string cmd, std::ostream &stream) override;

  double timeStamp () override;

  unsigned int getCpuCount (void) override;
  unsigned int getCurrentCpu (void) override;
  void setCurrentCpu (unsigned int) override;
  bool prepare (const std::vector<ITarget::ResumeType> &) override;
  bool resume (void) override;
  ITarget::WaitRes wait (std::vector<ResumeRes> &) override;
  bool halt (void) override;
  bool supportsTargetXML (void) override;
  virtual const char *getTargetXML (ByteView name __attribute__ ((unused))) override;

private:
  /// Structure for general and FP register info for the target description
  struct RegInfo
  {
    const char *name;
    const char *type;
  };

  // Fields in the DCSR register used here.
  /// \brief \c dcsr CSR \c step field.
  static const uint32_t DCSR_STEP = 0x00000004;
  /// \brief \c dcsr CSR \c step ebreak[msu] fields.
  static const uint32_t DCSR_EBREAKS = 0x0000b000;
  /// \brief \c dcsr CSR \c cause field.
  static const uint32_t DCSR_CAUSE = 0x000001C0;
  /// \brief \c dcsr CSR cause value for ebreak instruction was executed.
  static const uint32_t DCSR_CAUSE_EBREAK_EXECUTED = 0x00000040;

  /// \brief field indicating Hart 0 has stopped in \c haltsum0 DMI register.
  static const uint32_t HALTSUM_FIRST_HART = 0x00000001;

  /// \brief the GDB register number for the first integer register
  static const int REG_ZERO_GDBNUM = 0x0;
  /// \brief the GDB register number for the PC.
  static const int REG_PC_GDBNUM = 0x20;
  /// \brief the GDB register number for the first floating point register
  static const int REG_FT0_GDBNUM = 0x21;
  /// \brief the GDB register number for the first possible CSR
  static const int REG_CSR0_GDBNUM = 0x41;
  /// \brief the GDB register number for the last possible CSR
  static const int REG_CSR_LAST_GDBNUM = REG_CSR0_GDBNUM + 0xfff;

  /// Map of general registers
  std::map<const unsigned int, RegInfo> mGenRegMap {
    { REG_ZERO_GDBNUM,        { "zero", "int" }},
    { REG_ZERO_GDBNUM + 0x01, { "ra",   "code_ptr" }},
    { REG_ZERO_GDBNUM + 0x02, { "sp",   "data_ptr" }},
    { REG_ZERO_GDBNUM + 0x03, { "gp",   "data_ptr" }},
    { REG_ZERO_GDBNUM + 0x04, { "tp",   "data_ptr" }},
    { REG_ZERO_GDBNUM + 0x05, { "t0",   "int" }},
    { REG_ZERO_GDBNUM + 0x06, { "t1",   "int" }},
    { REG_ZERO_GDBNUM + 0x07, { "t2",   "int" }},
    { REG_ZERO_GDBNUM + 0x08, { "fp",   "data_ptr" }},
    { REG_ZERO_GDBNUM + 0x09, { "s1",   "int" }},
    { REG_ZERO_GDBNUM + 0x0a, { "a0",   "int" }},
    { REG_ZERO_GDBNUM + 0x0b, { "a1",   "int" }},
    { REG_ZERO_GDBNUM + 0x0c, { "a2",   "int" }},
    { REG_ZERO_GDBNUM + 0x0d, { "a3",   "int" }},
    { REG_ZERO_GDBNUM + 0x0e, { "a4",   "int" }},
    { REG_ZERO_GDBNUM + 0x0f, { "a5",   "int" }},
    { REG_ZERO_GDBNUM + 0x10, { "a6",   "int" }},
    { REG_ZERO_GDBNUM + 0x11, { "a7",   "int" }},
    { REG_ZERO_GDBNUM + 0x12, { "s2",   "int" }},
    { REG_ZERO_GDBNUM + 0x13, { "s3",   "int" }},
    { REG_ZERO_GDBNUM + 0x14, { "s4",   "int" }},
    { REG_ZERO_GDBNUM + 0x15, { "s5",   "int" }},
    { REG_ZERO_GDBNUM + 0x16, { "s6",   "int" }},
    { REG_ZERO_GDBNUM + 0x17, { "s7",   "int" }},
    { REG_ZERO_GDBNUM + 0x18, { "s8",   "int" }},
    { REG_ZERO_GDBNUM + 0x19, { "s9",   "int" }},
    { REG_ZERO_GDBNUM + 0x1a, { "s10",  "int" }},
    { REG_ZERO_GDBNUM + 0x1b, { "s11",  "int" }},
    { REG_ZERO_GDBNUM + 0x1c, { "t3",   "int" }},
    { REG_ZERO_GDBNUM + 0x1d, { "t4",   "int" }},
    { REG_ZERO_GDBNUM + 0x1e, { "t5",   "int" }},
    { REG_ZERO_GDBNUM + 0x1f, { "t6",   "int" }},
    { REG_PC_GDBNUM,          { "pc",   "code_ptr" }}};

  /// Map of floating point registers
  std::map<const unsigned int, RegInfo> mFpRegMap {
    { REG_FT0_GDBNUM,        { "ft0",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x01, { "ft1",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x02, { "ft2",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x03, { "ft3",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x04, { "ft4",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x05, { "ft5",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x06, { "ft6",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x07, { "ft7",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x08, { "fs0",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x09, { "fs1",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0a, { "fa0",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0b, { "fa1",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0c, { "fa2",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0d, { "fa3",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0e, { "fa4",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x0f, { "fa5",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x10, { "fa6",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x11, { "fa7",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x12, { "fs2",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x13, { "fs3",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x14, { "fs4",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x15, { "fs5",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x16, { "fs6",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x17, { "fs7",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x18, { "fs8",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x19, { "fs9",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1a, { "fs10", "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1b, { "fs11", "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1c, { "ft8",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1d, { "ft9",  "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1e, { "ft10", "ieee_single" }},
    { REG_FT0_GDBNUM + 0x1f, { "ft11", "ieee_single" }}};

  ITarget::WaitRes stepInstr (ITarget::ResumeRes &resumeRes);
  ITarget::WaitRes runToBreak (ITarget::ResumeRes &resumeRes);
  bool stoppedAtEbreak ();

  /// The XML target description
  char * mXmlTdesc;

  std::unique_ptr<Dmi> mDmi;
  uint64_t simStart;
  uint64_t clkPeriodNs;
  uint64_t mCpuTime;
  ITarget::ResumeType mRunAction;
  uint64_t mCycleCnt;
  uint64_t mInstrCnt;
  std::unique_ptr<Dmi::Dmstatus> mDmstatus;
};

}	// namespace EmbDebug

#endif
