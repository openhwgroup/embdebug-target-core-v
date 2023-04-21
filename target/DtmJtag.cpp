// Definition of a class to represent a JTAG Debug Transport Module
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#include <iomanip>
#include <iostream>

#include "DtmJtag.h"
#include "Utils.h"

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::setfill;
using std::setw;

/// \brief Constructor for the JTAG DTM.
///
/// This instantiates an underlying JTAG TAP, which in turn instantiates a
/// Verilator model of the MCU, and all the arguments are needed to create
/// that Verilator model.
///
/// We provide a plausible default value of mDmiWidth
/// \param[in] clkPeriodNs    \see VSim::VSim
/// \param[in] simTimeNs      \see VSim::VSim
/// \param[in] vcdFile        \see VSim::VSim
DtmJtag::DtmJtag (const vluint64_t clkPeriodNs, const vluint64_t simTimeNs,
                  const char *vcdFile)
    : mDmiWidth (42U)
{
  mTap.reset (new Tap (clkPeriodNs, simTimeNs, vcdFile));
}

/// \brief Destructor for the JTAG DTM.
///
/// While the JTAG TAP should be cleaned up automatically, we do this
/// explicitly as good practice.
DtmJtag::~DtmJtag ()
{
  mTap.reset (nullptr);
}

/// \brief Take the system through reset.
///
/// We need to reset the TAP and underlying processor model.  We then read
/// DTMCS in order to find out the count of cycles to spend in Run-Test/Idle
/// when accessing the same register more than once.
///
/// \todo Do we need to explicitly hard reset the DM interface?
///
/// \return \c true if we completed reset, \c false if the simulation
///         terminated during reset.
bool
DtmJtag::reset ()
{
  if (!mTap->reset ())
    return false; // Didn't complete reset

  // Read the DTM JTAG registers
  uint32_t idcode = readIdcode ();
  uint32_t dtmcs = readDtmcs ();

  // Update features of JTAG interface
  mTap->rtiCount (static_cast<uint8_t> ((dtmcs >> 12) & 0x7));
  uint8_t addrSize = static_cast<uint8_t> ((dtmcs >> 4) & 0x3f);
  mDmiWidth = 34U + addrSize;
  mDmiAddrMask = ~((~0ULL) << addrSize);

  // Log data
  cout << "IDCODE = 0x" << Utils::hexStr (idcode);
  cout << " [version = 0x" << ((idcode >> 28) & 0xf);
  cout << ", part # =  0x" << Utils::hexStr ((idcode >> 12) & 0xffff, 4);
  cout << ", manuf ID: 0x" << Utils::hexStr ((idcode >> 1) & 0x7ff, 3);
  cout << " [JEP106 ID = 0x" << Utils::hexStr ((idcode >> 1) & 0x7f, 2);
  cout << ", continuation # = " << ((idcode >> 8) & 0xf) << "]]" << endl;

  cout << "DTMCS = 0x" << Utils::hexStr (dtmcs);
  cout << " [dmihardreset = " << Utils::nonZero ((dtmcs >> 17) & 0x1);
  cout << ", dmireset = " << Utils::nonZero ((dtmcs >> 16) & 0x1);
  cout << ", idle = " << ((dtmcs >> 12) & 0x7);
  cout << ", dmistat = " << ((dtmcs >> 10) & 0x3);
  cout << ", abits = " << static_cast<uint32_t> (addrSize);
  cout << ", version = " << (dtmcs & 0xf);
  cout << ", addr mask = " << Utils::hexStr (mDmiAddrMask, 8) << "]" << endl;

  // Reset the DMI
  writeDtmcs (0x10000); // dmireset
  return true;          // Reset completed.
}

/// \brief Read a DMI register.
///
/// \param[in] address  DMI address from which to read.
/// \return Data read.
uint32_t
DtmJtag::dmiRead (uint64_t address)
{
  uint64_t reg = static_cast<uint64_t> (OP_READ);

  reg |= (address & mDmiAddrMask) << 34;
  mTap->writeReg (static_cast<uint8_t> (DMIACCESS), reg, mDmiWidth);

  while (true)
    {
      reg = mTap->readReg (static_cast<uint8_t> (DMIACCESS), mDmiWidth);
      if ((reg & 0x3ULL) == static_cast<uint64_t> (RES_RETRY))
        {
          cerr << "Warning dmiRead retry requested" << endl;
          writeDtmcs (0x10000); // dmireset
        }
      else
        break;
    }
  while ((reg & 0x3ULL) == static_cast<uint64_t> (RES_RETRY))
    ;

  if ((reg & 0x3ULL) != static_cast<uint64_t> (RES_OK))
    cerr << "Warning: unknown JTAG read result " << (reg & 0x3ULL)
         << ": ignored" << endl;

  return static_cast<uint32_t> ((reg >> 2) & 0xffffffffULL);
}

/// \brief Write a DMI register.
///
/// \param[in] address  DMI address from which to read.
/// \param[in] wdata    Data to write.
void
DtmJtag::dmiWrite (uint64_t address, uint32_t wdata)
{
  uint64_t reg = static_cast<uint64_t> (OP_WRITE);

  reg |= static_cast<uint64_t> (wdata) << 2;
  reg |= (address & mDmiAddrMask) << 34;
  mTap->writeReg (static_cast<uint8_t> (DMIACCESS), reg, mDmiWidth);

  while (true)
    {
      reg = mTap->readReg (static_cast<uint8_t> (DMIACCESS), mDmiWidth);
      if ((reg & 0x3ULL) == static_cast<uint64_t> (RES_RETRY))
        {
          cerr << "Warning dmiWrite retry requested" << endl;
          writeDtmcs (0x10000); // dmireset
        }
      else
        break;
    }

  if ((reg & 0x3ULL) != static_cast<uint64_t> (RES_OK))
    cerr << "Warning: unknown JTAG write result " << (reg & 0x3ULL)
         << ": ignored" << endl;
}

/// \brief Provide access to simulation time
///
/// \return The current simulation time in nanoseconds.
uint64_t
DtmJtag::simTimeNs () const
{
  return mTap->simTimeNs ();
}

/// \brief Read the IDCODE register.
///
/// This identifies the target, and is a simple read of a 32-bit register.
///
/// \return the IDCODE register.
uint32_t
DtmJtag::readIdcode ()
{
  return mTap->readReg (static_cast<uint8_t> (IDCODE), 32);
}

/// \brief Read the DTM Control and Status register.
///
/// This 32-bit register contains information about the debug module.
///
/// \return the DTMCS register.
uint32_t
DtmJtag::readDtmcs ()
{
  return mTap->readReg (static_cast<uint8_t> (DTMCS), 32);
}

/// \brief Write the DTM Control and Status register.
///
/// Only the dmihardreset and dmireset have meaning.
///
/// \param[in] val  The value to write in the DTMCS register.
void
DtmJtag::writeDtmcs (const uint32_t val)
{
  mTap->writeReg (static_cast<uint8_t> (DTMCS), val, 32);
}
