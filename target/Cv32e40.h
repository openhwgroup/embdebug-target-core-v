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

#include <memory>

using namespace EmbDebug;

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
  bool
  supportsTargetXML (void) override
  {
    return true;
  }
  virtual const char *
  getTargetXML (ByteView name) override
  {
    return nullptr;
  }

private:
  ITarget::WaitRes stepInstr (ITarget::ResumeRes &resumeRes);
  ITarget::WaitRes runToBreak (ITarget::ResumeRes &resumeRes);
  bool stoppedAtEbreak ();

  std::unique_ptr<Dmi> mDmi;
  uint64_t simStart;
  uint64_t clkPeriodNs;
  uint64_t mCpuTime;
  ITarget::ResumeType mRunAction;
  uint64_t mCycleCnt;
  uint64_t mInstrCnt;
  std::unique_ptr<Dmi::Dmstatus> mDmstatus;
};

#endif
