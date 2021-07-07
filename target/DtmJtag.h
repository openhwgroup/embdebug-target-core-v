// Declaration of a class to represent a JTAG Debug Transport Module
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#ifndef DTM_JTAG_H
#define DTM_JTAG_H

#include "IDtm.h"
#include "Tap.h"

/// \brief Abstract class for a Debug Transport Module
///
/// This must be subclassed by an actual implementation such as JTAG or USB.
class DtmJtag : public IDtm
{
public:
  // Constructor and destructor
  DtmJtag (const uint64_t clkPeriodNs, const uint64_t simTimeNs,
           const char *vcdFile);
  DtmJtag (const DtmJtag &) = delete;
  ~DtmJtag ();

  // API
  bool reset () override;
  virtual uint32_t dmiRead (uint64_t address) override;
  virtual void dmiWrite (uint64_t address, uint32_t wdata) override;
  virtual uint64_t simTimeNs () const override;

  // Delete the copy assignment operator
  DtmJtag &operator= (const DtmJtag &) = delete;

private:
  /// \brief Enumeration of the RISC-V JTAG TAP instruction register values
  ///
  /// All 5 bits long, hard-coded here
  enum IrReg
  {
    BYPASS0 = 0x00,
    IDCODE = 0x01,
    DTMCS = 0x10,
    DMIACCESS = 0x11,
    BYPASS1 = 0x1f,
  };

  /// \brief Enumeration of op field when writing
  enum Op
  {
    OP_NOP = 0,
    OP_READ = 1,
    OP_WRITE = 2,
    OP_RESERVED = 3,
  };

  /// \brief Eneration of op field when reading
  enum Res
  {
    RES_OK = 0,
    RES_RESERVED = 1,
    RES_ERROR = 2,
    RES_RETRY = 3,
  };

  /// \brief the JTAG Tap associated with this DTM
  std::unique_ptr<Tap> mTap;

  /// \brief the width of a DMI register
  ///
  /// 2 bits of operation, 32 bits of data, <abits> of address, where <abits>
  /// is provided by the DTM Control & Status register.
  uint8_t mDmiWidth;

  /// \brief Mask for DM interface address
  ///
  /// The speci allows for up to 64 bits, although in practice it will be much
  /// smaller.
  uint64_t mDmiAddrMask;

  // Helper methods
  uint32_t readIdcode ();
  uint32_t readDtmcs ();
  void writeDtmcs (const uint32_t val);
};

#endif // DTM_JTAG_H
