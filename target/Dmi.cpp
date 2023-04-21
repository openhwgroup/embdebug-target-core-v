// Definition of the generic Debug Module Interface
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Dmi.h"
#include "Utils.h"

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::max;
using std::min;
using std::ostream;
using std::setfill;
using std::setw;
using std::size_t;
using std::unique_ptr;

/// \brief Constructor for the DMI
///
/// The DTM has to be constructed locally, but is then passed to us, and we
/// take ownership.  It is a unique_ptr, so the call to the constructor must
/// explicitly move the pointer.
///
/// We create local instances of all the interesting registers
///
/// \param[in] dtm_  The Debug Transport Module we will use.
Dmi::Dmi (unique_ptr<IDtm> dtm_) : mDtm (std::move (dtm_))
{
  mData.reset (new Data (mDtm));
  mDmcontrol.reset (new Dmcontrol (mDtm));
  mDmstatus.reset (new Dmstatus (mDtm));
  mHartinfo.reset (new Hartinfo (mDtm));
  mHaltsum.reset (new Haltsum (mDtm));
  mHawindowsel.reset (new Hawindowsel (mDtm));
  mHawindow.reset (new Hawindow (mDtm));
  mAbstractcs.reset (new Abstractcs (mDtm));
  mCommand.reset (new Command (mDtm));
  mAbstractauto.reset (new Abstractauto (mDtm));
  mConfstrptr.reset (new Confstrptr (mDtm));
  mNextdm.reset (new Nextdm (mDtm));
  mProgbuf.reset (new Progbuf (mDtm));
  mAuthdata.reset (new Authdata (mDtm));
  mSbaddress.reset (new Sbaddress (mDtm));
  mSbcs.reset (new Sbcs (mDtm));
  mSbdata.reset (new Sbdata (mDtm));
}

/// \brief Select a hart
///
/// \param[in] h  Number of the hart to select
void
Dmi::selectHart (uint32_t h)
{
  mDmcontrol->reset ();
  mDmcontrol->hartsel (h);
  mDmcontrol->dmactive (true);
  mDmcontrol->write ();
}

/// Report the maximum of hart number supported
///
/// This uses the algorithm specified in section 3.3 in the ratified Debug
/// Spec v 0.13.2.
///
/// \return The number of harts which exist
uint32_t
Dmi::hartsellen ()
{
  selectHart (mDmcontrol->hartselMax ());
  mDmcontrol->reset ();
  return mDmcontrol->hartsel ();
}

/// \brief Select and halt a hart
///
/// \param[in] h  Number of the hart to select and halt
void
Dmi::haltHart (uint32_t h)
{
  mDmcontrol->reset ();
  mDmcontrol->haltreq (true);
  mDmcontrol->hartsel (h);
  mDmcontrol->dmactive (true);
  mDmcontrol->write ();
}

/// \brief Get a CSR's name from its address
///
/// \param[in] csrAddr  The address of the CSR
/// \return  The name of the CSR, or "UNKNOWN" if it does not exist
const char *
Dmi::csrName (const uint16_t csrAddr) const
{
  try
    {
      return mCsrMap.at (csrAddr).name;
    }
  catch (const std::out_of_range &oor)
    {
      return "UNKNOWN";
    }
}

/// \brief Get whether a CSR is readonly from its address
///
/// \note There is no error indication for this call.
///
/// \param[in] csrAddr  The group the CSR belongs to
/// \return  \c true if the CSR is read only, or if it does not exist, false
///          otherwise.
bool
Dmi::csrReadOnly (const uint16_t csrAddr) const
{
  try
    {
      return mCsrMap.at (csrAddr).readOnly;
    }
  catch (const std::out_of_range &oor)
    {
      return true;
    }
}

/// \brief Get a CSR's type from its address
///
/// \param[in] csrAddr  The group the CSR belongs to
/// \return  The group of the CSR, or "NONE" if it does not exist
Dmi::CsrType
Dmi::csrType (const uint16_t csrAddr) const
{
  try
    {
      return mCsrMap.at (csrAddr).type;
    }
  catch (const std::out_of_range &oor)
    {
      return NONE;
    }
}

/// \brief Read a CSR.
///
/// \param[in]  addr  Address of the CSR to read.
/// \param[out] res   The result of the read - only valid if there is no error.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::readCsr (uint16_t addr, uint32_t &res)
{
  mCommand->reset ();
  mCommand->cmdtype (Dmi::Command::ACCESS_REG);
  mCommand->aarsize (Dmi::Command::ACCESS32);
  mCommand->aatransfer (true);
  mCommand->aawrite (false);
  mCommand->aaregno (addr);
  mCommand->write ();

  // Check for any error
  mAbstractcs->read ();
  Abstractcs::CmderrVal err = mAbstractcs->cmderr ();

  switch (err)
    {
    case Abstractcs::CMDERR_NONE:
      // Good result, read the data into the result
      mData->read (0);
      res = mData->data (0);
      return err;

    case Abstractcs::CMDERR_BUSY:
      // Debug unit is busy. This should not happen. Reset the hart, then the
      // debug unit.
      //
      // TODO We should allow retry.

      // Toggle ndmreset
      for (bool flag : { true, false })
        {
          mDmcontrol->reset ();
          mDmcontrol->ndmreset (flag);
          mDmcontrol->write ();
        }

      // Toggle dmactive
      for (bool flag : { false, true })
        {
          mDmcontrol->reset ();
          mDmcontrol->dmactive (flag);
          mDmcontrol->write ();
        }

      return err;

    default:
      // Otherwise bad result, clear the error. If the error is "busy", then
      // we may have to reset the hart and then the debug unit.
      mAbstractcs->cmderrClear ();
      mAbstractcs->write ();
      return err;
    }
};

/// \brief Write a CSR.
///
/// \note There is no check that the CSR is writable.
///
/// \param[in] addr  Address of the CSR to write.
/// \param[in] val   The value to write to the CSR.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::writeCsr (uint16_t addr, uint32_t val)
{
  mData->reset (0);
  mData->data (0, val);
  mData->write (0);

  mCommand->reset ();
  mCommand->cmdtype (Dmi::Command::ACCESS_REG);
  mCommand->aarsize (Dmi::Command::ACCESS32);
  mCommand->aatransfer (true);
  mCommand->aawrite (true);
  mCommand->aaregno (addr);
  mCommand->write ();

  // Check for any error
  mAbstractcs->read ();
  Abstractcs::CmderrVal err = mAbstractcs->cmderr ();

  switch (err)
    {
    case Abstractcs::CMDERR_NONE:
      // Good result, nothing to do.
      return err;

    case Abstractcs::CMDERR_BUSY:
      // Debug unit is busy. This should not happen. Reset the hart, then the
      // debug unit.
      //
      // TODO We should allow retry.

      // Toggle ndmreset
      for (bool flag : { true, false })
        {
          mDmcontrol->reset ();
          mDmcontrol->ndmreset (flag);
          mDmcontrol->write ();
        }

      // Toggle dmactive
      for (bool flag : { false, true })
        {
          mDmcontrol->reset ();
          mDmcontrol->dmactive (flag);
          mDmcontrol->write ();
        }

      return err;

    default:
      // Otherwise bad result, clear the error. If the error is "busy", then
      // we may have to reset the hart and then the debug unit.
      mAbstractcs->cmderrClear ();
      mAbstractcs->write ();
      return err;
    }
};

/// \brief Read a general purpose register
///
/// \param[in] regNum  Number of the register to read.
/// \param[out] res   The result of the read - only valid if there is no error.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::readGpr (size_t regNum, uint32_t &res)
{
  Abstractcs::CmderrVal err
      = readCsr (GPR_BASE + static_cast<uint16_t> (regNum), res);
  return err;
}

/// \brief Write a general purpose register
///
/// \param[in] regNum  Number of the register to write.
/// \param[in] val     The value to write to the register.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::writeGpr (size_t regNum, uint32_t val)
{
  return writeCsr (GPR_BASE + static_cast<uint16_t> (regNum), val);
}

/// \brief Read a floating point register
///
/// \param[in] regNum  Number of the register to read.
/// \param[out] res   The result of the read - only valid if there is no error.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::readFpr (size_t regNum, uint32_t &res)
{
  uint16_t csrNum = FPR_BASE + static_cast<uint16_t> (regNum);
  Abstractcs::CmderrVal err = readCsr (csrNum, res);
  return err;
}

/// \brief Write a floating point register
///
/// \param[in] regNum  Number of the register to write.
/// \param[in] val     The value to write to the register.
/// \return  The error code for the access.
Dmi::Abstractcs::CmderrVal
Dmi::writeFpr (size_t regNum, uint32_t val)
{
  uint16_t csrNum = FPR_BASE + static_cast<uint16_t> (regNum);
  return writeCsr (csrNum, val);
}

/// \brief Read from memory
///
/// We can't use the abstract command approach, since the MemoryAccess command
/// is not implemented. For now we use the System Bus.
///
/// \note This is problematic, since the system bus only permits 32-bit reads,
///       potentially troublesome for volatile memory locations.
///
/// \todo There is a known hardware design issue, where system bus accesses
///       always succeed, even if there is no memory at the location. Reads
///       will return 0xbadcab1e.
///
/// \param[in]  addr    Address to read from
/// \param[in]  nBytes  Number of bytes to read
/// \param[out] buf     Buffer for storing the bytes read
/// \return  The error code for the access.
Dmi::Sbcs::SberrorVal
Dmi::readMem (uint64_t addr, size_t nBytes, unique_ptr<uint8_t[]> &buf)
{
  uint32_t startAddr = addr & 0xfffffffc;
  uint32_t endAddr = (addr + nBytes + 3) & 0xfffffffc;
  size_t nWords = (endAddr - startAddr) / 4;
  size_t bufIndex = 0;
  uint32_t w;

  // Set up systembus to read on setting the address or reading the data and
  // autoincrement if we need to read more than one word
  mSbcs->reset ();
  mSbcs->sbreadonaddr (true);
  mSbcs->sbaccess (Sbcs::SBACCESS_32);
  mSbcs->sbautoincrement (nWords > 1);
  mSbcs->sbreadondata (true);
  mSbcs->sberrorClear ();
  mSbcs->write ();

  // Initial word, which may be different from the actual start address if the
  // start is misaligned. Setting the address will cause the first read.
  mSbaddress->reset (0);
  mSbaddress->sbaddress (0, startAddr);
  mSbaddress->write (0);

  do
    mSbcs->read ();
  while (mSbcs->sbbusy ());

  Sbcs::SberrorVal err = mSbcs->sberror ();
  if (err != Sbcs::SBERR_NONE)
    return err;

  // Get the read data, which will trigger the next read if we have
  // autoincrement set.
  mSbdata->read (0);
  w = mSbdata->sbdata (0);

  size_t offset = (addr - startAddr);
  for (size_t i = offset; i < min (offset + nBytes, static_cast<size_t> (4)); i++)
    {
      uint8_t ch = static_cast<uint8_t> ((w >> (8 * i)) & 0xff);
      buf[bufIndex++] = ch;
    }

  startAddr += 4;
  if (startAddr == endAddr) // Read was just a single word or less
    return Sbcs::SBERR_NONE;

  // Bulk of the words
  for (; startAddr < (endAddr - 4); startAddr += 4)
    {
      do
        mSbcs->read ();
      while (mSbcs->sbbusy ());

      err = mSbcs->sberror ();
      if (err != Sbcs::SBERR_NONE)
        return err;

      mSbdata->read (0);
      w = mSbdata->sbdata (0);

      for (size_t i = 0; i < 4; i++)
        {
	  uint8_t ch = static_cast<uint8_t> (w & 0xff);
          buf[bufIndex++] = ch;
          w = w >> 8;
        }
    }

  // Final word, which may be different the actual end address if the
  // end is misaligned
  do
    mSbcs->read ();
  while (mSbcs->sbbusy ());

  err = mSbcs->sberror ();
  if (err != Sbcs::SBERR_NONE)
    return err;

  mSbdata->read (0);
  w = mSbdata->sbdata (0);

  offset = endAddr - (addr + nBytes);
  for (size_t i = 0; i < 4 - offset; i++)
    {
      uint8_t ch = static_cast<uint8_t> (w & 0xff);
      buf[bufIndex++] = ch;
      w = w >> 8;
    }

  return Sbcs::SBERR_NONE;
}

/// \brief Write to memory
///
/// We can't use the abstract command approach, since the MemoryAccess command
/// is not implemented. For now we use the System Bus.
///
/// \note This is problematic, since the system bus only permits 32-bit writes,
///       potentially troublesome for volatile memory locations.
///
/// \todo There is a known hardware design issue, where system bus accesses
///       always succeed, even if there is no memory at the location.
///
/// \param[in  addr    Address to write to
/// \param[in] nBytes  Number of bytes to write
/// \param[in] buf     Buffer with the bytes to write
/// \return  The error code for the access.
Dmi::Sbcs::SberrorVal
Dmi::writeMem (uint64_t addr, size_t nBytes, unique_ptr<uint8_t[]> &buf)
{
  uint32_t startAddr = addr & 0xfffffffc;
  uint32_t endAddr = (addr + nBytes + 3) & 0xfffffffc;
  bool startAligned = startAddr == addr;
  bool endAligned = endAddr == (addr + nBytes);
  size_t nWords = (endAddr - startAddr) / 4;
  size_t bufIndex = 0;
  uint32_t w;

  // Set up systembus
  // - we read on setting the address if the initial word is misaligned
  // - we don't read on reading the data
  // - we will set autoincrement if we need to write more than one word, but
  //   not until we have done the initial word read if necessary.
  mSbcs->reset ();
  mSbcs->sbreadonaddr (!startAligned);
  mSbcs->sbaccess (Sbcs::SBACCESS_32);
  mSbcs->sbautoincrement (nWords > 1);
  mSbcs->sbreadondata (false);
  mSbcs->sberrorClear ();
  mSbcs->write ();

  // Initial word, which may be different from the actual start address if the
  // start is misaligned. If we are misaligned this will read the first word.
  mSbaddress->reset (0);
  mSbaddress->sbaddress (0, startAddr);
  mSbaddress->write (0);

  // If we are misaligned read data at the first word into w.
  if (!startAligned)
    {
      do
        mSbcs->read ();
      while (mSbcs->sbbusy ());

      Sbcs::SberrorVal err = mSbcs->sberror ();
      if (err != Sbcs::SBERR_NONE)
        return err;

      // Get the read data
      mSbdata->read (0);
      w = mSbdata->sbdata (0);

      // Clear the read on address flag if set and reset the start address
      mSbcs->reset ();
      mSbcs->sbreadonaddr (false);
      mSbcs->sbaccess (Sbcs::SBACCESS_32);
      mSbcs->sbautoincrement (nWords > 1);
      mSbcs->sbreadondata (false);
      mSbcs->sberrorClear ();
      mSbcs->write ();

      mSbaddress->reset (0);
      mSbaddress->sbaddress (0, startAddr);
      mSbaddress->write (0);
    }

  // Create the first word to write
  size_t offset = (addr - startAddr);
  for (size_t i = offset; i < min (offset + nBytes, static_cast<std::size_t> (4)); i++)
    {
      w &= ~((0x000000ff) << (8 * i));
      w |= static_cast<uint32_t> (buf[bufIndex++]) << (8 * i);
    }

  // Write the value into sbdata, which will trigger the write and wait for it
  // to complete.
  mSbdata->sbdata (0, w);
  mSbdata->write (0);

  do
    mSbcs->read ();
  while (mSbcs->sbbusy ());

  Sbcs::SberrorVal err = mSbcs->sberror ();
  if (err != Sbcs::SBERR_NONE)
    return err;

  startAddr += 4;
  if (startAddr == endAddr) // Write was just a single word or less
    return Sbcs::SBERR_NONE;

  // Write the bulk of the words
  for (; startAddr < (endAddr - 4); startAddr += 4)
    {
      // Create the word
      w = 0;
      for (size_t i = 0; i < 4; i++)
        w |= static_cast<uint32_t> (buf[bufIndex++]) << (i * 8);

      // Write the value into sbdata, which will trigger the write and wait
      // for it to complete.
      mSbdata->sbdata (0, w);
      mSbdata->write (0);

      do
        mSbcs->read ();
      while (mSbcs->sbbusy ());

      err = mSbcs->sberror ();
      if (err != Sbcs::SBERR_NONE)
        return err;
    }

  // Final word, which may be different to the actual end address if the
  // end is misaligned

  // Set the read on address flag if the end is misaligned
  if (!endAligned)
    {
      mSbcs->reset ();
      mSbcs->sbreadonaddr (true);
      mSbcs->sbaccess (Sbcs::SBACCESS_32);
      mSbcs->sbautoincrement (false);
      mSbcs->sbreadondata (false);
      mSbcs->sberrorClear ();
      mSbcs->write ();

      // Trigger a read by writing the current address
      mSbaddress->reset (0);
      mSbaddress->sbaddress (0, startAddr);
      mSbaddress->write (0);

      do
        mSbcs->read ();
      while (mSbcs->sbbusy ());

      err = mSbcs->sberror ();
      if (err != Sbcs::SBERR_NONE)
        return err;

      // Get the read data
      mSbdata->read (0);
      w = mSbdata->sbdata (0);

      // Clear the read on address flag if set
      mSbcs->reset ();
      mSbcs->sbreadonaddr (false);
      mSbcs->sbaccess (Sbcs::SBACCESS_32);
      mSbcs->sbautoincrement (nWords > 1);
      mSbcs->sbreadondata (false);
      mSbcs->sberrorClear ();
      mSbcs->write ();
    }

  // Create the word to write and then write it.
  offset = endAddr - (addr + nBytes);
  for (size_t i = 0; i < 4 - offset; i++)
    {
      w &= ~(0x000000ff << (8 * i));
      w |= static_cast<uint32_t> (buf[bufIndex++]) << (8 * i);
    }

  // Write the value into sbdata, which will trigger the write and wait for it
  // to complete.
  mSbdata->sbdata (0, w);
  mSbdata->write (0);

  do
    mSbcs->read ();
  while (mSbcs->sbbusy ());

  err = mSbcs->sberror ();
  return err;
}

/// \brief Reset the underlying DTM.
void
Dmi::dtmReset ()
{
  mDtm->reset ();
}

/// \brief Provide access to simulation time
///
/// \return The current simulation time in nanoseconds.
uint64_t
Dmi::simTimeNs () const
{
  return mDtm->simTimeNs ();
}

/// \brief Get the \p data register.
///
/// \return  An instance of class Data:: representing the set of \c data
///          registers.
std::unique_ptr<Dmi::Data> &
Dmi::data ()
{
  return mData;
}

/// \brief Get the \p dmcontrol register.
///
/// \return  An instance of class Dmcontrol:: representing the \c dmcontrol
///          register.
std::unique_ptr<Dmi::Dmcontrol> &
Dmi::dmcontrol ()
{
  return mDmcontrol;
}

/// \brief Get the \p dmstatus register.
///
/// \return  An instance of class Dmstatus:: representing the \c dmstatus
///          register.
std::unique_ptr<Dmi::Dmstatus> &
Dmi::dmstatus ()
{
  return mDmstatus;
}

/// \brief Get the \p hartinfo register.
///
/// \return  An instance of class Hartinfo:: representing the \c hartinfo
///          register.
std::unique_ptr<Dmi::Hartinfo> &
Dmi::hartinfo ()
{
  return mHartinfo;
}

/// \brief Get the \p haltsum register.
///
/// \return  An instance of class Haltsum:: representing the set of \c haltsum
///          registers.
std::unique_ptr<Dmi::Haltsum> &
Dmi::haltsum ()
{
  return mHaltsum;
}

/// \brief Get the \p hawindowsel register.
///
/// \return  An instance of class Hawindowsel:: representing the \c hawindowsel
///          register.
std::unique_ptr<Dmi::Hawindowsel> &
Dmi::hawindowsel ()
{
  return mHawindowsel;
}

/// \brief Get the \p hawindow register.
///
/// \return  An instance of class Hawindow:: representing the \c hawindow
///          register.
std::unique_ptr<Dmi::Hawindow> &
Dmi::hawindow ()
{
  return mHawindow;
}

/// \brief Get the \p abstractcs register.
///
/// \return  An instance of class Abstractcs:: representing the \c abstractcs
///          register.
std::unique_ptr<Dmi::Abstractcs> &
Dmi::abstractcs ()
{
  return mAbstractcs;
}

/// \brief Get the \p command register.
///
/// \return  An instance of class Command:: representing the \c command
///          register.
std::unique_ptr<Dmi::Command> &
Dmi::command ()
{
  return mCommand;
}

/// \brief Get the \p abstractauto register.
///
/// \return  An instance of class Abstractauto:: representing the
///          \c abstractauto register.
std::unique_ptr<Dmi::Abstractauto> &
Dmi::abstractauto ()
{
  return mAbstractauto;
}

/// \brief Get the \p constrptr register.
///
/// \return  An instance of class Constrptr:: representing the set of \c
///          confstrptr registers.
std::unique_ptr<Dmi::Confstrptr> &
Dmi::confstrptr ()
{
  return mConfstrptr;
}

/// \brief Get the \p nextdm register.
///
/// \return  An instance of class Nextdm:: representing the \c nextdm
///          register.
std::unique_ptr<Dmi::Nextdm> &
Dmi::nextdm ()
{
  return mNextdm;
}

/// \brief Get the \p progbuf register.
///
/// \return  An instance of class Progbuf:: representing the set of \c progbuf
///          registers.
std::unique_ptr<Dmi::Progbuf> &
Dmi::progbuf ()
{
  return mProgbuf;
}

/// \brief Get the \p authdata register.
///
/// \return  An instance of class Authdata:: representing the \c authdata
///          register.
std::unique_ptr<Dmi::Authdata> &
Dmi::authdata ()
{
  return mAuthdata;
}

/// \brief Get the \p sbaddress register.
///
/// \return  An instance of class Sbaddress:: representing the set of
///          \c sbaddress registers.
std::unique_ptr<Dmi::Sbaddress> &
Dmi::sbaddress ()
{
  return mSbaddress;
}

/// \brief Get the \p sbcs register.
///
/// \return  An instance of class Sbcs:: representing the \c sbcs
///          register.
std::unique_ptr<Dmi::Sbcs> &
Dmi::sbcs ()
{
  return mSbcs;
}

/// \brief Get the \p sbdata register.
///
/// \return  An instance of class Sbdata:: representing the set of
///          \c sbdata registers.
std::unique_ptr<Dmi::Sbdata> &
Dmi::sbdata ()
{
  return mSbdata;
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Data class                                                         *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Data class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Data::Data (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mDataReg[i] = 0x0;
}

/// \brief Read the value of the specified abstract \c data register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c data register to read.
void
Dmi::Data::read (const size_t n)
{
  if (n < NUM_REGS)
    mDataReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading data[" << n << "] invalid: ignored." << endl;
}

/// \brief Set the specified abstract \c data register to its reset value.
void
Dmi::Data::reset (const size_t n)
{
  if (n < NUM_REGS)
    mDataReg[n] = RESET_VALUE;
  else
    cerr << "Warning: reseting data[" << n << "] invalid: ignored." << endl;
}

/// \brief Write the value of the specified abstract \c data register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Data::write (const size_t n)
{
  if (n < NUM_REGS)
    mDtm->dmiWrite (DMI_ADDR[n], mDataReg[n]);
  else
    cerr << "Warning: writing data[" << n << "] invalid: ignored." << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Data::DMI_ADDR[Dmi::Data::NUM_REGS];

/// Get the value the specified \c data register.
///
/// \param[in] n  Index of the \c data register to get.
/// \return  The value in the specified \c data register.
uint32_t
Dmi::Data::data (const size_t n) const
{
  if (n < NUM_REGS)
    return mDataReg[n];
  else
    {
      cerr << "Warning: getting data[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// Set the value the specified \c data register.
///
/// \param[in] n        Index of the \c data register to get.
/// \param[in] dataVal  The value to be set in the specified \c data register.
void
Dmi::Data::data (const size_t n, const uint32_t dataVal)
{
  if (n < NUM_REGS)
    mDataReg[n] = dataVal;
  else
    cerr << "Warning: setting data[" << n << "] invalid: ignored." << endl;
}

/// \brief Output operator for the Dmi::Data class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Data> &p)
{
  std::ostringstream oss ("[");

  for (size_t i = 0; i < p->NUM_REGS; i++)
    oss << Utils::hexStr (p->mDataReg[i], 8)
        << ((i == (p->NUM_REGS - 1)) ? "]" : ", ");

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Dmcontrol class                                                    *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Dmcontrol class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Dmcontrol::Dmcontrol (unique_ptr<IDtm> &dtm_)
    : mCurrentHartsel (0), mPrettyPrint (false), mDtm (dtm_),
      mDmcontrolReg (Dmi::Dmcontrol::RESET_VALUE)
{
}

/// \brief Read the value of the \c dmcontrol register.
///
/// The register is refreshed via the DTM.
void
Dmi::Dmcontrol::read ()
{
  mDmcontrolReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c dmcontrol register to its reset value.
///
/// \note This includes setting the \c hartsel field to its most recently
///       selected value.
void
Dmi::Dmcontrol::reset ()
{
  mDmcontrolReg = RESET_VALUE;
  hartsel (mCurrentHartsel);
}

/// \brief Write the value of the \c dmcontrol register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Dmcontrol::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mDmcontrolReg);
}

/// \brief Control whether to pretty print the \c dmcontrol register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Dmcontrol::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// \brief Set the \c haltreq bit in \c dmcontrol.
///
/// \note Value set will apply to the \em new value of \c hartsel.
///
/// \param[in] flag  \c true sets the halt request bit for all currently
///                  selected harts, so that running hards will halt whenever
///                  their halt request bit is set.\c false clears the halt
///                  request bit,
void
Dmi::Dmcontrol::haltreq (const bool flag)
{
  if (flag)
    mDmcontrolReg |= HALTREQ_MASK;
  else
    mDmcontrolReg &= ~HALTREQ_MASK;
}

/// \brief Set the \c resumereq bit in \c dmcontrol to 1.
void
Dmi::Dmcontrol::resumereq ()
{
  mDmcontrolReg |= RESUMEREQ_MASK;
}

/// \brief Get the \c hartreset bit in \c dmcontrol (NOT IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Always returns
///          \c false.
///
/// \returns Always returns \c false.
bool
Dmi::Dmcontrol::hartreset () const
{
  return false;
}

/// \brief Set the \c hartreset bit in \c dmcontrol (NOT IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Ignored with a
///          warning.
///
/// \param[in] flag  Value ignored.
void
Dmi::Dmcontrol::hartreset (const bool flag __attribute ((unused)))
{
  cerr << "Warning: Setting dmcontrol:hartreset not supported: ignored."
       << endl;
}

/// \brief Set the \c ackhavereset bit in \c dmcontrol to 1.
void
Dmi::Dmcontrol::ackhavereset ()
{
  mDmcontrolReg |= ACKHAVERESET_MASK;
}

/// \brief Get the \c hasel bit in \c dmcontrol (NOT IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Always returns
///          \c false (only ever one selected hart).
///
/// \return Always returns \c false.
bool
Dmi::Dmcontrol::hasel () const
{
  return false;
}

/// \brief Set the \c hasel bit in \c dmcontrol (NOT IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Ignored with a
///          warning.
///
/// \param[in] flag  Value ignored.
void
Dmi::Dmcontrol::hasel (const bool flag __attribute ((unused)))
{
  cerr << "Warning: Setting dmcontrol:hasel not supported: ignored." << endl;
}

/// \brief Get the \c hartsello and \c hartselhi bits of \c dmcontrol
///
/// Computes \c hartsel as \c hartselhi << 10 | \c hartsello.
///
/// \return The value of \c hartsel
uint32_t
Dmi::Dmcontrol::hartsel () const
{
  uint32_t hartsello = (mDmcontrolReg & (HARTSELLO_MASK)) >> HARTSELLO_OFFSET;
  uint32_t hartselhi = (mDmcontrolReg & (HARTSELHI_MASK)) >> HARTSELHI_OFFSET;
  return (hartselhi << HARTSELLO_SIZE) | hartsello;
}

/// \brief Set the \c hartsello and \c hartselhi bits of \c dmcontrol
///
/// Sets \c hartslehi to \p (hartselVal >> 10) & 0x3ff
/// Sets \c hartsello to \p hartselVal & 0x3ff
///
/// Also remembers this is the currently selected hart for use when reseting.
///
/// \param[in] hartselVal  The value of \c hartsel to set.
void
Dmi::Dmcontrol::hartsel (const uint32_t hartselVal)
{
  if (hartselVal >= (1 << (HARTSELLO_SIZE + HARTSELHI_SIZE)))
    cerr << "Warning: requested value of hartsel, " << hartselVal
         << ", exceeds the maximum permitted value: higher bits ignored."
         << endl;

  mCurrentHartsel = hartselVal;

  uint32_t hartsello = (hartselVal << HARTSELLO_OFFSET) & HARTSELLO_MASK;
  uint32_t hartselhi
      = ((hartselVal >> HARTSELLO_SIZE) << HARTSELHI_OFFSET) & HARTSELHI_MASK;
  mDmcontrolReg &= ~(HARTSELLO_MASK | HARTSELHI_MASK);
  mDmcontrolReg |= hartselhi | hartsello;
}

/// \brief Return the maximum possible value of Hartsel
///
/// \return The maximum value for Hartsel
uint32_t
Dmi::Dmcontrol::hartselMax ()
{
  return ((HARTSELHI_MASK >> HARTSELHI_OFFSET) << HARTSELLO_SIZE)
         | (HARTSELLO_MASK >> HARTSELLO_OFFSET);
}
/// \brief Set the \c setresethaltreq bit in \c dmcontrol to 1 (NOT
///        IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Ignored with a
///          warning.
void
Dmi::Dmcontrol::setresethaltreq ()
{
  cerr << "Warning: Setting dmcontrol:setresethaltreq not supported: ignored."
       << endl;
}

/// \brief Set the \c clrresethaltreq bit in \c dmcontrol to 1 (NOT
///        IMPLEMENTED).
///
/// \warning Not implemented for the CORE-V MCU debug unit. Ignored with a
///          warning.
void
Dmi::Dmcontrol::clrresethaltreq ()
{
  cerr << "Warning: Setting dmcontrol:clrresethaltreq not supported: ignored."
       << endl;
}

/// \brief Get the \c ndmreset bit in \c dmcontrol
///
/// return \c true if the \c ndmreset bit is set and \c false otherwise.
bool
Dmi::Dmcontrol::ndmreset () const
{
  return (mDmcontrolReg & NDMRESET_MASK) != 0;
}

/// \brief Set the \c ndmreset bit in \c dmcontrol
///
/// \param[in] flag  If \c true sets the \c ndmreset bit, otherwise clears the
///                  \c ndmreset bit.
void
Dmi::Dmcontrol::ndmreset (const bool flag)
{
  if (flag)
    mDmcontrolReg |= NDMRESET_MASK;
  else
    mDmcontrolReg &= ~NDMRESET_MASK;
}

/// \brief Get the \c dmactive bit in \c dmcontrol
///
/// return \c true if the \c dmactive bit is set and \c false otherwise.
bool
Dmi::Dmcontrol::dmactive () const
{
  return (mDmcontrolReg & DMACTIVE_MASK) != 0;
}

/// \brief Set the \c dmactive bit in \c dmcontrol
///
/// \param[in] flag  If \c true sets the \c dmactive bit, otherwise clears the
///                  \c dmactive bit.
void
Dmi::Dmcontrol::dmactive (const bool flag)
{
  if (flag)
    mDmcontrolReg |= DMACTIVE_MASK;
  else
    mDmcontrolReg &= ~DMACTIVE_MASK;
}

/// \brief Output operator for the Dmi::Dmcontrol class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Dmcontrol> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    oss << "[ haltreq = " << Utils::nonZero (p->mDmcontrolReg & p->HALTREQ_MASK)
        << ", resumereq = "
        << Utils::nonZero (p->mDmcontrolReg & p->RESUMEREQ_MASK)
        << ", hartreset = " << Utils::boolStr (p->hartreset ())
        << ", ackhavereset = "
        << Utils::nonZero (p->mDmcontrolReg & p->ACKHAVERESET_MASK)
        << ", hasel = " << Utils::boolStr (p->hasel ()) << ", hartsel = 0x"
        << Utils::hexStr (p->hartsel (), 5) << ", setresethaltreq = "
        << Utils::nonZero (p->mDmcontrolReg & p->SETRESETHALTREQ_MASK)
        << ", clrresethaltreq = "
        << Utils::nonZero (p->mDmcontrolReg & p->CLRRESETHALTREQ_MASK)
        << ", ndmreset = " << Utils::boolStr (p->ndmreset ())
        << ", dmactive = " << Utils::boolStr (p->dmactive ()) << " ]";
  else
    oss << Utils::hexStr (p->mDmcontrolReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Dmstatus class                                                     *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Dmstatus class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Dmstatus::Dmstatus (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_), mDmstatusReg (0)
{
}

/// \brief Read the value of the \c dmstatus register.
///
/// The register is refreshed via the DTM.
void
Dmi::Dmstatus::read ()
{
  mDmstatusReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Control whether to pretty print the \c dmstatus register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Dmstatus::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// Get the \c impebreak field of the \c dmstatus register.
///
/// \return \c true if the \c impebreak field of \c dmstatus is set, \c false
///         otherwise.
bool
Dmi::Dmstatus::impebreak () const
{
  return (mDmstatusReg & IMPEBREAK_MASK) != 0;
}

/// Get the \c havereset fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allhavereset of \c anyhavereset
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::havereset () const
{
  return (mDmstatusReg & (ALLHAVERESET_MASK | ANYHAVERESET_MASK)) != 0;
}

/// Get the \c resumeack fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allresumeack of \c anyresumeack
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::resumeack () const
{
  return (mDmstatusReg & (ALLRESUMEACK_MASK | ANYRESUMEACK_MASK)) != 0;
}

/// Get the \c nonexistent fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allnonexistent of \c anynonexistent
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::nonexistent () const
{
  return (mDmstatusReg & (ALLNONEXISTENT_MASK | ANYNONEXISTENT_MASK)) != 0;
}

/// Get the \c unavail fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allunavail of \c anyunavail
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::unavail () const
{
  return (mDmstatusReg & (ALLUNAVAIL_MASK | ANYUNAVAIL_MASK)) != 0;
}

/// Get the \c running fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allrunning of \c anyrunning
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::running () const
{
  return (mDmstatusReg & (ALLRUNNING_MASK | ANYRUNNING_MASK)) != 0;
}

/// Get the \c halted fields of the \c dmstatus register.
///
/// \note we do not distinguish between "all" or "any" versions of the flag.
///
/// \return \c true if either of the \c allhalted of \c anyhalted
///         fields of \c dmstatus is set, \c false otherwise.
bool
Dmi::Dmstatus::halted () const
{
  return (mDmstatusReg & (ALLHALTED_MASK | ANYHALTED_MASK)) != 0;
}

/// Get the \c authenticated field of the \c dmstatus register.
///
/// \return \c true if the \c authenticated field of \c dmstatus is set,
///         \c false otherwise.
bool
Dmi::Dmstatus::authenticated () const
{
  return (mDmstatusReg & AUTHENTICATED_MASK) != 0;
}

/// Get the \c authbusy field of the \c dmstatus register.
///
/// \return \c true if the \c authbusy field of \c dmstatus is set, \c false
///         otherwise.
bool
Dmi::Dmstatus::authbusy () const
{
  return (mDmstatusReg & AUTHBUSY_MASK) != 0;
}

/// Get the \c hasresethaltreq field of the \c dmstatus register.
///
/// \return \c true if the \c hasresethaltreq field of \c dmstatus is set,
///         \c false otherwise.
bool
Dmi::Dmstatus::hasresethaltreq () const
{
  return (mDmstatusReg & HASRESETHALTREQ_MASK) != 0;
}

/// Get the \c confstrptrvalid field of the \c dmstatus register.
///
/// \return \c true if the \c confstrptrvalid field of \c dmstatus is set,
///         \c false otherwise.
bool
Dmi::Dmstatus::confstrptrvalid () const
{
  return (mDmstatusReg & CONFSTRPTRVALID_MASK) != 0;
}

/// Get the \c version field of the \c dmstatus register.
///
/// \return the value in the \c version field of \c dmstatus.
uint8_t
Dmi::Dmstatus::version () const
{
  return (mDmstatusReg & VERSION_MASK) >> VERSION_OFFSET;
}

/// \brief Output operator for the Dmi::Dmstatus class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Dmstatus> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    oss << "[ impebreak = " << Utils::boolStr (p->impebreak ())
        << ", havereset = " << Utils::boolStr (p->havereset ())
        << ", resumeack = " << Utils::boolStr (p->resumeack ())
        << ", nonexistent = " << Utils::boolStr (p->nonexistent ())
        << ", unavail = " << Utils::boolStr (p->unavail ())
        << ", running = " << Utils::boolStr (p->running ())
        << ", halted = " << Utils::boolStr (p->halted ())
        << ", authenticated = " << Utils::boolStr (p->authenticated ())
        << ", authbusy = " << Utils::boolStr (p->authbusy ())
        << ", hasresethaltreq = " << Utils::boolStr (p->hasresethaltreq ())
        << ", confstrptrvalid = " << Utils::boolStr (p->confstrptrvalid ())
        << ", version = " << static_cast<uint16_t> (p->version ()) << " ]";
  else
    oss << Utils::hexStr (p->mDmstatusReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Hartinfo class                                                     *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Hartinfo class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Hartinfo::Hartinfo (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_), mHartinfoReg (0)
{
}

/// \brief Read the value of the \c hartinfo register.
///
/// The register is refreshed via the DTM.
void
Dmi::Hartinfo::read ()
{
  mHartinfoReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Control whether to pretty print the \c hartinfo register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Hartinfo::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// Get the \c nscratch field of the \c hartinfo register.
///
/// \return  The value in the \c nscratch field of the \c hartinfo register
uint8_t
Dmi::Hartinfo::nscratch () const
{
  return (mHartinfoReg & NSCRATCH_MASK) >> NSCRATCH_OFFSET;
}

/// Get the \c dataaccess field of the \c hartinfo register.
///
/// \return  \c true if \c dataaccess field of \c hartinfo is set, \c false
///          otherwise.
bool
Dmi::Hartinfo::dataaccess () const
{
  return (mHartinfoReg & DATAACCESS_MASK) != 0;
}

/// Get the \c datasize field of the \c hartinfo register.
///
/// \return  The value in the \c datasize field of the \c hartinfo register
uint8_t
Dmi::Hartinfo::datasize () const
{
  return (mHartinfoReg & DATASIZE_MASK) >> DATASIZE_OFFSET;
}

/// Get the \c dataaddr field of the \c hartinfo register.
///
/// \return  The value in the \c dataaddr field of the \c hartinfo register
uint16_t
Dmi::Hartinfo::dataaddr () const
{
  return (mHartinfoReg & DATAADDR_MASK) >> DATAADDR_OFFSET;
}

/// \brief Output operator for the Dmi::Hartinfo class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Hartinfo> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    oss << "[ nscratch = " << static_cast<uint16_t> (p->nscratch ())
        << ", dataaccess = " << Utils::boolStr (p->dataaccess ())
        << ", datasize = " << static_cast<uint16_t> (p->datasize ())
        << ", dataaddr = 0x" << Utils::hexStr (p->dataaddr (), 3) << " ]";
  else
    oss << Utils::hexStr (p->mHartinfoReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Haltsum class                                                      *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Haltsum class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Haltsum::Haltsum (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mHaltsumReg[i] = 0x0;
}

/// \brief Read the value of the specified \c haltsum register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c haltsum register to read.
void
Dmi::Haltsum::read (const size_t n)
{
  if (n < NUM_REGS)
    mHaltsumReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading haltsum[" << n << "] invalid: ignored." << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Haltsum::DMI_ADDR[Dmi::Haltsum::NUM_REGS];

/// Get the value the specified \c haltsum register.
///
/// \param[in] n  Index of the \c haltsum register to get.
/// \return  The value in the specified \c haltsum register.
uint32_t
Dmi::Haltsum::haltsum (const size_t n) const
{
  if (n < NUM_REGS)
    return mHaltsumReg[n];
  else
    {
      cerr << "Warning: getting haltsum[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// \brief Output operator for the Dmi::Haltsum class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Haltsum> &p)
{
  std::ostringstream oss;
  oss << "[" << hex << setw (8) << setfill ('0');

  for (size_t i = 0; i < p->NUM_REGS; i++)
    {
      oss << p->mHaltsumReg[i];
      if (i != (p->NUM_REGS - 1))
        oss << ", ";
    }

  oss << "]";
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Hawindowsel class                                                  *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Hawindowsel class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Hawindowsel::Hawindowsel (unique_ptr<IDtm> &dtm_)
    : mDtm (dtm_), mHawindowselReg (Dmi::Hawindowsel::RESET_VALUE)
{
}

/// \brief Read the value of the \c hawindowsel register.
///
/// The register is refreshed via the DTM.
void
Dmi::Hawindowsel::read ()
{
  mHawindowselReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c hawindowsel register to its reset value.
void
Dmi::Hawindowsel::reset ()
{
  mHawindowselReg = RESET_VALUE;
}

/// \brief Write the value of the \c hawindowsel register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Hawindowsel::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mHawindowselReg);
}

/// \brief Get the \c hawindowsel bits of \c hawindowsel
///
/// \return The value of \c hawindowsel
uint16_t
Dmi::Hawindowsel::hawindowsel () const
{
  return static_cast<uint16_t> ((mHawindowselReg & HAWINDOWSEL_MASK)
                                >> HAWINDOWSEL_OFFSET);
}

/// \brief Set the \c hawindowsel bits of \c hawindowsel
///
/// \param[in] hawindowselVal  The value of \c hawindowsel to set.
void
Dmi::Hawindowsel::hawindowsel (const uint16_t hawindowselVal)
{
  if (hawindowselVal >= (1 << HAWINDOWSEL_SIZE))
    cerr << "Warning: requested value of hawindowsel, " << hawindowselVal
         << ", exceeds the maximum permitted value: higher bits ignored."
         << endl;

  mHawindowselReg &= ~HAWINDOWSEL_MASK;
  mHawindowselReg |= (hawindowselVal << HAWINDOWSEL_OFFSET) & HAWINDOWSEL_MASK;
}

/// \brief Output operator for the Dmi::Hawindowsel class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Hawindowsel> &p)
{
  std::ostringstream oss;
  oss << Utils::hexStr (p->mHawindowselReg, 8);
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Hawindow class                                                     *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Hawindow class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Hawindow::Hawindow (unique_ptr<IDtm> &dtm_)
    : mDtm (dtm_), mHawindowReg (Dmi::Hawindow::RESET_VALUE)
{
}

/// \brief Read the value of the \c hawindow register.
///
/// The register is refreshed via the DTM.
void
Dmi::Hawindow::read ()
{
  mHawindowReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c hawindow register to its reset value.
void
Dmi::Hawindow::reset ()
{
  mHawindowReg = RESET_VALUE;
}

/// \brief Write the value of the \c hawindow register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Hawindow::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mHawindowReg);
}

/// \brief Get the value of \c hawindow
///
/// \return The value of \c hawindow
uint32_t
Dmi::Hawindow::hawindow () const
{
  return mHawindowReg;
}

/// \brief Set the value of \c hawindow
///
/// \param[in] hawindowVal  The value of \c hawindow to set.
void
Dmi::Hawindow::hawindow (const uint32_t hawindowVal)
{
  mHawindowReg = hawindowVal;
}

/// \brief Output operator for the Dmi::Hawindow class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Hawindow> &p)
{
  std::ostringstream oss;
  oss << Utils::hexStr (p->mHawindowReg, 8);
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Abstractcs class                                                   *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Abstractcs class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Abstractcs::Abstractcs (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_),
      mAbstractcsReg (Dmi::Abstractcs::RESET_VALUE)
{
}

/// \brief Read the value of the \c abstractcs register.
///
/// The register is refreshed via the DTM.
void
Dmi::Abstractcs::read ()
{
  mAbstractcsReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c abstractcs register to its reset value.
void
Dmi::Abstractcs::reset ()
{
  mAbstractcsReg = RESET_VALUE;
}

/// \brief Write the value of the \c abstractcs register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Abstractcs::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mAbstractcsReg);
}

/// \brief Control whether to pretty print the \c abstractcs register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Abstractcs::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// \brief Get the \c progbufsize bits of \c abstractcs
///
/// \return The value of \c progbufsize
uint8_t
Dmi::Abstractcs::progbufsize () const
{
  return static_cast<uint8_t> ((mAbstractcsReg & PROGBUFSIZE_MASK)
                               >> PROGBUFSIZE_OFFSET);
}

/// \brief Get the \c busy bit of \c abstractcs
///
/// \return \c true if the \c busy bit is set, \c false otherwise.
bool
Dmi::Abstractcs::busy () const
{
  return (mAbstractcsReg & BUSY_MASK) != 0;
}

/// \brief Get the \c cmderr bits of \c abstractcs
///
/// \return The value of \c cmderr
Dmi::Abstractcs::CmderrVal
Dmi::Abstractcs::cmderr () const
{
  switch (static_cast<int> ((mAbstractcsReg & CMDERR_MASK) >> CMDERR_OFFSET))
    {
    case CMDERR_NONE:
      return CMDERR_NONE;
    case CMDERR_BUSY:
      return CMDERR_BUSY;
    case CMDERR_UNSUPPORTED:
      return CMDERR_UNSUPPORTED;
    case CMDERR_EXCEPT:
      return CMDERR_EXCEPT;
    case CMDERR_HALT_RESUME:
      return CMDERR_HALT_RESUME;
    case CMDERR_BUS:
      return CMDERR_BUS;
    case CMDERR_OTHER:
      return CMDERR_OTHER;

    default:
      return CMDERR_UNKNOWN;
    }
}

/// \brief Get the value of cmderr as a string.
///
/// \return  The constant string corresponding to the cmderr
const char *
Dmi::Abstractcs::cmderrName (Dmi::Abstractcs::CmderrVal err)
{
  switch (err)
    {
    case Dmi::Abstractcs::CMDERR_NONE:
      return "None";
    case Dmi::Abstractcs::CMDERR_BUSY:
      return "Busy";
    case Dmi::Abstractcs::CMDERR_UNSUPPORTED:
      return "Unsupported";
    case Dmi::Abstractcs::CMDERR_EXCEPT:
      return "Exception";
    case Dmi::Abstractcs::CMDERR_HALT_RESUME:
      return "Halt/resume";
    case Dmi::Abstractcs::CMDERR_BUS:
      return "Bus error";
    case Dmi::Abstractcs::CMDERR_OTHER:
      return "Other";

    default:
      return "???";
    }
}

/// \brief Clear the \c cmderr bits of \c abstractcs
///
/// This means setting all the bits to 1, prior to a write.
void
Dmi::Abstractcs::cmderrClear ()
{
  mAbstractcsReg |= CMDERR_MASK;
}

/// \brief Get the \c datacount bits of \c abstractcs
///
/// \return The value of \c datacount
uint8_t
Dmi::Abstractcs::datacount () const
{
  return static_cast<uint8_t> ((mAbstractcsReg & DATACOUNT_MASK)
                               >> DATACOUNT_OFFSET);
}

/// \brief Output operator for the Dmi::Abstractcs class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Abstractcs> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    {
      Dmi::Abstractcs::CmderrVal err = p->cmderr ();
      oss << "[ progbufsize = " << static_cast<uint16_t> (p->progbufsize ())
          << ", busy = " << Utils::boolStr (p->busy ())
          << ", cmderr = " << static_cast<uint16_t> (err) << " ("
          << Dmi::Abstractcs::cmderrName (err) << ")"
          << ", datacount = 0x" << static_cast<uint16_t> (p->datacount ())
          << " ]";
    }
  else
    oss << Utils::hexStr (p->mAbstractcsReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Command class                                                      *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Command class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Command::Command (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_), mCommandReg (Dmi::Command::RESET_VALUE)
{
}

/// \brief Set the \c command register to its reset value.
void
Dmi::Command::reset ()
{
  mCommandReg = RESET_VALUE;
}

/// \brief Write the value of the \c command register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Command::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mCommandReg);
}

/// \brief Control whether to pretty print the \c command register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Command::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// \brief Set the \c cmdtype bits of \c command
///
/// \param[in] cmdtypeVal  The value of \c cmdType to set.  Permitted values
///                        are \c ACCESS_REG (0), \c QUICK_ACCESS (1) or
///                        \c ACCESS_MEM (2). Any other value is ignored with
///                        a warning.
void
Dmi::Command::cmdtype (const Dmi::Command::CmdtypeEnum cmdtypeVal)
{
  switch (cmdtypeVal)
    {
    case ACCESS_REG:
    case QUICK_ACCESS:
    case ACCESS_MEM:
      mCommandReg &= ~CMDTYPE_MASK;
      mCommandReg |= (static_cast<uint32_t> (cmdtypeVal) << CMDTYPE_OFFSET)
                     & CMDTYPE_MASK;
      return;

    default:
      cerr << "Warning: " << cmdtypeVal
           << " not valid for cmdtype field: ignored" << endl;
      return;
    }
}

/// \brief Set the \c control bits of \c command
///
/// \param[in] controlVal  The value of \c control to set.
void
Dmi::Command::control (const uint32_t controlVal)
{
  if (controlVal >= (1 << CONTROL_SIZE))
    cerr << "Warning: requested value of control, " << controlVal
         << ", exceeds the maximum permitted value: higher bits ignored."
         << endl;

  mCommandReg &= ~CONTROL_MASK;
  mCommandReg
      |= (static_cast<uint32_t> (controlVal) << CONTROL_OFFSET) & CONTROL_MASK;
}

/// \brief Set the \c aamvirtual bit for the \c command \c control field.
///
/// \param[in] flag  \c true if the bit is to be set, false otherwise.
void
Dmi::Command::aamvirtual (bool flag)
{
  if (flag)
    mCommandReg |= AAMVIRTUAL_MASK;
  else
    mCommandReg &= ~AAMVIRTUAL_MASK;
}

/// \brief Set the \c aarsize bits for the \c command \c control field.
///
/// \param[in] aarsizeval  The value to be set, permitted values are
///                       \c ACCESS32 (2), \c ACCESS32 (3) or \c ACCESS32 (4).
///                       Any other value is ignored with a warning.
void
Dmi::Command::aarsize (const Dmi::Command::AasizeEnum aarsizeVal)
{
  switch (aarsizeVal)
    {
    case ACCESS32:
    case ACCESS64:
    case ACCESS128:
      mCommandReg &= ~AARSIZE_MASK;
      mCommandReg |= static_cast<uint32_t> (aarsizeVal) << AARSIZE_OFFSET;
      return;

    default:
      cerr << "Warning: " << aarsizeVal
           << " not valid for aarsize field: ignored" << endl;
      return;
    }
}

/// \brief Set the \c aamsize bits for the \c command \c control field.
///
/// \param[in] aamsizeval  The value to be set, permitted values are
///                       \c ACCESS8 (0), \c ACCESS16 (1), \c ACCESS32 (2),
///                       \c ACCESS32 (3) or \c ACCESS32 (4).  Any other value
///                       is ignored with a warning.
void
Dmi::Command::aamsize (const Dmi::Command::AasizeEnum aamsizeVal)
{
  switch (aamsizeVal)
    {
    case ACCESS8:
    case ACCESS16:
    case ACCESS32:
    case ACCESS64:
    case ACCESS128:
      mCommandReg &= ~AAMSIZE_MASK;
      mCommandReg |= static_cast<uint32_t> (aamsizeVal) << AAMSIZE_OFFSET;
      return;

    default:
      cerr << "Warning: " << aamsizeVal
           << " not valid for aamsize field: ignored" << endl;
      return;
    }
}

/// \brief Set the \c postincrement bit for the \c command \c control field.
///
/// \param[in] flag  \c true if the bit is to be set, false otherwise.
void
Dmi::Command::aapostincrement (const bool flag)
{
  if (flag)
    mCommandReg |= AAPOSTINCREMENT_MASK;
  else
    mCommandReg &= ~AAPOSTINCREMENT_MASK;
}

/// \brief Set the \c postexec bit for the \c command \c control field.
///
/// \param[in] flag  \c true if the bit is to be set, false otherwise.
void
Dmi::Command::aapostexec (const bool flag)
{
  if (flag)
    mCommandReg |= POSTEXEC_MASK;
  else
    mCommandReg &= ~POSTEXEC_MASK;
}

/// \brief Set the \c transfer bit for the \c command \c control field.
///
/// \param[in] flag  \c true if the bit is to be set, false otherwise.
void
Dmi::Command::aatransfer (const bool flag)
{
  if (flag)
    mCommandReg |= TRANSFER_MASK;
  else
    mCommandReg &= ~TRANSFER_MASK;
}

/// \brief Set the \c write bit for the \c command \c control field.
///
/// \param[in] flag  \c true if the bit is to be set, false otherwise.
void
Dmi::Command::aawrite (const bool flag)
{
  if (flag)
    mCommandReg |= WRITE_MASK;
  else
    mCommandReg &= ~WRITE_MASK;
}

/// \brief Set the \c target-specific bits for the \c command \c control
///        field.
///
/// \param[in] value  The value to be set. Must be in the range 0-3.
void
Dmi::Command::aatargetSpecific (uint8_t val)
{
  if (val > 3)
    {
      cerr << "Warning: " << val
           << " too large for target-specific field: ignored" << endl;
      return;
    }
  else
    {
      mCommandReg &= ~TARGET_SPECIFIC_MASK;
      mCommandReg |= static_cast<uint32_t> (val) << TARGET_SPECIFIC_OFFSET;
    }
}

/// \brief Set the \c regno bits for the \c command \c control field.
///
/// \param[in] value  The value to be set.
void
Dmi::Command::aaregno (uint16_t val)
{
  mCommandReg &= ~REGNO_MASK;
  mCommandReg |= static_cast<uint32_t> (val) << REGNO_OFFSET;
}

/// \brief Output operator for the Dmi::Command class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Command> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    oss << "[ cmdtype = "
        << ((p->mCommandReg & p->CMDTYPE_MASK) >> p->CMDTYPE_OFFSET)
        << ", control = 0x" << hex << setw (6) << setfill ('0')
        << ((p->mCommandReg & p->CONTROL_MASK) >> p->CONTROL_OFFSET) << " ]";
  else
    oss << Utils::hexStr (p->mCommandReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Abstractauto class                                                 *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Abstractauto class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Abstractauto::Abstractauto (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_),
      mAbstractautoReg (Dmi::Abstractauto::RESET_VALUE)
{
}

/// \brief Read the value of the \c abstractauto register.
///
/// The register is refreshed via the DTM.
void
Dmi::Abstractauto::read ()
{
  mAbstractautoReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c abstractauto register to its reset value.
void
Dmi::Abstractauto::reset ()
{
  mAbstractautoReg = RESET_VALUE;
}

/// \brief Write the value of the \c abstractauto register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Abstractauto::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mAbstractautoReg);
}

/// \brief Control whether to pretty print the \c abstractauto register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Abstractauto::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// \brief Get the \c autoexecprogbuf bits of \c abstractauto.
///
/// \return  The value of the \c autoexecprogbuf bits of \c abstractauto
uint16_t
Dmi::Abstractauto::autoexecprogbuf () const
{
  return static_cast<uint16_t> ((mAbstractautoReg & AUTOEXECPROGBUF_MASK)
                                << AUTOEXECPROGBUF_OFFSET);
}

/// \brief Set the \c autoexecprogbuf bits of \c abstractauto.
///
/// \param[in] autoexecprogbufVal  The value of the \c autoexecprogbuf bits to
///                                set in \c abstractauto.
void
Dmi::Abstractauto::autoexecprogbuf (const uint16_t autoexecprogbufVal)
{
  mAbstractautoReg &= ~AUTOEXECPROGBUF_MASK;
  mAbstractautoReg
      |= (static_cast<uint32_t> (autoexecprogbufVal) << AUTOEXECPROGBUF_OFFSET)
         & AUTOEXECPROGBUF_MASK;
}

/// \brief Get the \c autoexecdata bits of \c abstractauto.
///
/// \return  The value of the \c autoexecdata bits of \c abstractauto
uint16_t
Dmi::Abstractauto::autoexecdata () const
{
  return static_cast<uint16_t> ((mAbstractautoReg & AUTOEXECDATA_MASK)
                                << AUTOEXECDATA_OFFSET);
}

/// \brief Set the \c autoexecdata bits of \c abstractauto.
///
/// \param[in] autoexecdataVal  The value of the \c autoexecdata bits to set in
///                        \c abstractauto.
void
Dmi::Abstractauto::autoexecdata (const uint16_t autoexecdataVal)
{
  if (autoexecdataVal >= (1 << AUTOEXECDATA_SIZE))
    cerr << "Warning: requested value of autoexecdata, " << autoexecdataVal
         << ", exceeds the maximum permitted value: higher bits ignored."
         << endl;

  mAbstractautoReg &= ~AUTOEXECDATA_MASK;
  mAbstractautoReg
      |= (static_cast<uint32_t> (autoexecdataVal) << AUTOEXECDATA_OFFSET)
         & AUTOEXECDATA_MASK;
}

/// \brief Output operator for the Dmi::Abstractauto class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Abstractauto> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    oss << "[ autoexecprogbuf = 0x" << hex << setw (4) << setfill ('0')
        << p->autoexecprogbuf () << ", autoexecdata = 0x" << setw (3)
        << p->autoexecdata () << " ]";
  else
    oss << Utils::hexStr (p->mAbstractautoReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Confstrptr class                                                   *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Confstrptr class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Confstrptr::Confstrptr (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mConfstrptrReg[i] = 0x0;
}

/// \brief Read the value of the specified \c confstrptr register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c confstrptr register to read.
void
Dmi::Confstrptr::read (const size_t n)
{
  if (n < NUM_REGS)
    mConfstrptrReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading confstrptr[" << n << "] invalid: ignored."
         << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Confstrptr::DMI_ADDR[Dmi::Confstrptr::NUM_REGS];

/// Get the value the specified \c confstrptr register.
///
/// \param[in] n  Index of the \c confstrptr register to get.
/// \return  The value in the specified \c confstrptr register.
uint32_t
Dmi::Confstrptr::confstrptr (const size_t n) const
{
  if (n < NUM_REGS)
    return mConfstrptrReg[n];
  else
    {
      cerr << "Warning: getting confstrptr[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// \brief Output operator for the Dmi::Confstrptr class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Confstrptr> &p)
{
  std::ostringstream oss;
  oss << "[" << hex << setw (8) << setfill ('0');

  for (size_t i = 0; i < p->NUM_REGS; i++)
    {
      oss << p->mConfstrptrReg[i];
      if (i != (p->NUM_REGS - 1))
        oss << ", ";
    }

  oss << "]";
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Nextdm class                                                       *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Nextdm class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Nextdm::Nextdm (unique_ptr<IDtm> &dtm_)
    : mDtm (dtm_), mNextdmReg (Dmi::Nextdm::RESET_VALUE)
{
}

/// \brief Read the value of the \c nextdm register.
///
/// The register is refreshed via the DTM.
void
Dmi::Nextdm::read ()
{
  mNextdmReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Get the value of \c nextdm
///
/// \return The value of \c nextdm
uint32_t
Dmi::Nextdm::nextdm () const
{
  return mNextdmReg;
}

/// \brief Output operator for the Dmi::Nextdm class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Nextdm> &p)
{
  std::ostringstream oss;
  oss << Utils::hexStr (p->mNextdmReg, 8);
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Progbuf class                                                      *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Progbuf class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Progbuf::Progbuf (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mProgbufReg[i] = 0x0;
}

/// \brief Read the value of the specified abstract \c progbuf register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c progbuf register to read.
void
Dmi::Progbuf::read (const size_t n)
{
  if (n < NUM_REGS)
    mProgbufReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading progbuf[" << n << "] invalid: ignored." << endl;
}

/// \brief Set the specified abstract \c progbuf register to its reset value.
void
Dmi::Progbuf::reset (const size_t n)
{
  if (n < NUM_REGS)
    mProgbufReg[n] = RESET_VALUE;
  else
    cerr << "Warning: reseting progbuf[" << n << "] invalid: ignored." << endl;
}

/// \brief Write the value of the specified abstract \c progbuf register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Progbuf::write (const size_t n)
{
  if (n < NUM_REGS)
    mDtm->dmiWrite (DMI_ADDR[n], mProgbufReg[n]);
  else
    cerr << "Warning: writing progbuf[" << n << "] invalid: ignored." << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Progbuf::DMI_ADDR[Dmi::Progbuf::NUM_REGS];

/// Get the value the specified \c progbuf register.
///
/// \param[in] n  Index of the \c progbuf register to get.
/// \return  The value in the specified \c progbuf register.
uint32_t
Dmi::Progbuf::progbuf (const size_t n) const
{
  if (n < NUM_REGS)
    return mProgbufReg[n];
  else
    {
      cerr << "Warning: getting progbuf[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// Set the value the specified \c progbuf register.
///
/// \param[in] n         Index of the \c progbuf register to get.
/// \param[in] progbufValy  The value to be set in the specified \c progbuf
/// register.
void
Dmi::Progbuf::progbuf (const size_t n, const uint32_t progbufVal)
{
  if (n < NUM_REGS)
    mProgbufReg[n] = progbufVal;
  else
    cerr << "Warning: setting progbuf[" << n << "] invalid: ignored." << endl;
}

/// \brief Output operator for the Dmi::Progbuf class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Progbuf> &p)
{
  std::ostringstream oss ("[");

  for (size_t i = 0; i < p->NUM_REGS; i++)
    oss << p->mProgbufReg[i] << ((i == (p->NUM_REGS - 1)) ? "]" : ", ");

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Authdata class                                                     *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Authdata class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Authdata::Authdata (unique_ptr<IDtm> &dtm_)
    : mDtm (dtm_), mAuthdataReg (Dmi::Authdata::RESET_VALUE)
{
}

/// \brief Read the value of the \c authdata register.
///
/// The register is refreshed via the DTM.
void
Dmi::Authdata::read ()
{
  cerr << "Warning: authentication not supported while reading authdata"
       << endl;
  mAuthdataReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c authdata register to its reset value.
void
Dmi::Authdata::reset ()
{
  cerr << "Warning: authentication not supported while reseting authdata"
       << endl;
  mAuthdataReg = RESET_VALUE;
}

/// \brief Write the value of the \c authdata register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Authdata::write ()
{
  cerr << "Warning: authentication not supported while writing authdata"
       << endl;
  mDtm->dmiWrite (DMI_ADDR, mAuthdataReg);
}

/// \brief Get the value of \c authdata
///
/// \return The value of \c authdata
uint32_t
Dmi::Authdata::authdata () const
{
  cerr << "Warning: authentication not supported while getting authdata"
       << endl;
  return mAuthdataReg;
}

/// \brief Set the value of \c authdata
///
/// \param[in] authdataVal  The value of \c authdata to set.
void
Dmi::Authdata::authdata (const uint32_t authdataVal __attribute__ ((unused)))
{
  cerr << "Warning: authentication not supported while setting authdata: "
       << "value ignored" << endl;
}

/// \brief Output operator for the Dmi::Authdata class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Authdata> &p)
{
  std::ostringstream oss;
  oss << Utils::hexStr (p->mAuthdataReg, 8);
  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Sbaddress class                                                    *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Sbaddress class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Sbaddress::Sbaddress (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mSbaddressReg[i] = 0x0;
}

/// \brief Read the value of the specified abstract \c sbaddress register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c sbaddress register to read.
void
Dmi::Sbaddress::read (const size_t n)
{
  if (n < NUM_REGS)
    mSbaddressReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading sbaddress[" << n << "] invalid: ignored." << endl;
}

/// \brief Set the specified abstract \c sbaddress register to its reset value.
void
Dmi::Sbaddress::reset (const size_t n)
{
  if (n < NUM_REGS)
    mSbaddressReg[n] = RESET_VALUE;
  else
    cerr << "Warning: reseting sbaddress[" << n << "] invalid: ignored."
         << endl;
}

/// \brief Write the value of the specified abstract \c sbaddress register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Sbaddress::write (const size_t n)
{
  if (n < NUM_REGS)
    mDtm->dmiWrite (DMI_ADDR[n], mSbaddressReg[n]);
  else
    cerr << "Warning: writing sbaddress[" << n << "] invalid: ignored." << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Sbaddress::DMI_ADDR[Dmi::Sbaddress::NUM_REGS];

/// Get the value the specified \c sbaddress register.
///
/// \param[in] n  Index of the \c sbaddress register to get.
/// \return  The value in the specified \c sbaddress register.
uint32_t
Dmi::Sbaddress::sbaddress (const size_t n) const
{
  if (n < NUM_REGS)
    return mSbaddressReg[n];
  else
    {
      cerr << "Warning: getting sbaddress[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// Set the value the specified \c sbaddress register.
///
/// \param[in] n         Index of the \c sbaddress register to get.
/// \param[in] sbaddressValy  The value to be set in the specified \c sbaddress
/// register.
void
Dmi::Sbaddress::sbaddress (const size_t n, const uint32_t sbaddressVal)
{
  if (n < NUM_REGS)
    mSbaddressReg[n] = sbaddressVal;
  else
    cerr << "Warning: setting sbaddress[" << n << "] invalid: ignored." << endl;
}

/// \brief Output operator for the Dmi::Sbaddress class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Sbaddress> &p)
{
  std::ostringstream oss ("[");

  for (size_t i = 0; i < p->NUM_REGS; i++)
    oss << p->mSbaddressReg[i] << ((i == (p->NUM_REGS - 1)) ? "]" : ", ");

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Sbcs class                                                         *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Sbcs class.
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Sbcs::Sbcs (unique_ptr<IDtm> &dtm_)
    : mPrettyPrint (false), mDtm (dtm_), mSbcsReg (Dmi::Sbcs::RESET_VALUE)
{
}

/// \brief Read the value of the \c sbcs register.
///
/// The register is refreshed via the DTM.
void
Dmi::Sbcs::read ()
{
  mSbcsReg = mDtm->dmiRead (DMI_ADDR);
}

/// \brief Set the \c sbcs register to its reset value.
void
Dmi::Sbcs::reset ()
{
  mSbcsReg = RESET_VALUE;
}

/// \brief Write the value of the \c sbcs register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Sbcs::write ()
{
  mDtm->dmiWrite (DMI_ADDR, mSbcsReg);
}

/// \brief Control whether to pretty print the \c sbcs register.
///
/// @param[in] flag  If \c true, subsequent stream output will generate a list
///                  of field values, if false a simple hex value.
void
Dmi::Sbcs::prettyPrint (const bool flag)
{
  mPrettyPrint = flag;
}

/// \brief Get the \c sbversion bits in \c sbcs.
///
/// \return  The value in the \c sbversion bits of \c sbcs.
uint8_t
Dmi::Sbcs::sbversion () const
{
  return static_cast<uint8_t> ((mSbcsReg & SBVERSION_MASK) >> SBVERSION_OFFSET);
}

/// \brief Get the \c sbbusyerror bit in \c sbcs.
///
/// \return  \c true if the \c sbbusyerror bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbbusyerror () const
{
  return (mSbcsReg & SBBUSYERROR_MASK) != 0;
}

/// \brief Clear the \c sbbusyerror bit in \c sbcs.
///
/// Writing 1 to this bit clears it.
void
Dmi::Sbcs::sbbusyerrorClear ()
{
  mSbcsReg |= SBBUSYERROR_MASK;
}

/// \brief Get the \c sbbusy bit in \c sbcs.
///
/// \return  \c true if the \c sbbusy bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbbusy () const
{
  return (mSbcsReg & SBBUSY_MASK) != 0;
}

/// \brief Get the \c sbreadonaddr bit in \c sbcs.
///
/// \return  \c true if the \c sbreadonaddr bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbreadonaddr () const
{
  return (mSbcsReg & SBREADONADDR_MASK) != 0;
}

/// \brief Set the \c sbreadonaddr bit in \c sbcs.
///
/// \param[in] flag  If \c true sets \c sbreadonaddr bit in \c sbcs, otherwise
///                  clears it.
void
Dmi::Sbcs::sbreadonaddr (const bool flag)
{
  if (flag)
    mSbcsReg |= SBREADONADDR_MASK;
  else
    mSbcsReg &= ~SBREADONADDR_MASK;
}

/// \brief Get the \c sbaccess bits of \c sbcs.
///
/// \return  The value of the \c sbaccess bits of \c sbcs.
Dmi::Sbcs::SbaccessVal
Dmi::Sbcs::sbaccess () const
{
  SbaccessVal val = static_cast<SbaccessVal> ((mSbcsReg & SBACCESS_MASK)
                                              >> SBACCESS_OFFSET);

  switch (val)
    {
    case SBACCESS_8:
    case SBACCESS_16:
    case SBACCESS_32:
    case SBACCESS_64:
    case SBACCESS_128:
      return val;

    default:
      return SBACCESS_UNKNOWN;
    }
}

/// \brief Set the \c sbaccess bits in \c sbcs.
///
/// \param[in] val  Value to set for the \c sb bits in \c sbcs.
void
Dmi::Sbcs::sbaccess (const uint8_t val)
{
  if (val >= (1 << SBACCESS_SIZE))
    cerr << "Warning: " << val << " too large for sbaccess field of sbcs: "
         << "truncated" << endl;
  mSbcsReg &= ~SBACCESS_MASK;
  mSbcsReg |= (static_cast<uint32_t> (val) << SBACCESS_OFFSET) & SBACCESS_MASK;
}

/// \brief Get the \c sbautoincrement bit in \c sbcs.
///
/// \return  \c true if the \c sbautoincrement bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbautoincrement () const
{
  return (mSbcsReg & SBAUTOINCREMENT_MASK) != 0;
}

/// \brief Set the \c sbautoincrement bit in \c sbcs.
///
/// \param[in] flag  If \c true sets \c sbautoincrement bit in \c sbcs,
///                  otherwise clears it.
void
Dmi::Sbcs::sbautoincrement (const bool flag)
{
  if (flag)
    mSbcsReg |= SBAUTOINCREMENT_MASK;
  else
    mSbcsReg &= ~SBAUTOINCREMENT_MASK;
}

/// \brief Get the \c sbreadondata bit in \c sbcs.
///
/// \return  \c true if the \c sbreadondata bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbreadondata () const
{
  return (mSbcsReg & SBREADONDATA_MASK) != 0;
}

/// \brief Set the \c sbreadondata bit in \c sbcs.
///
/// \param[in] flag  If \c true sets \c sbreadondata bit in \c sbcs, otherwise
///                  clears it.
void
Dmi::Sbcs::sbreadondata (const bool flag)
{
  if (flag)
    mSbcsReg |= SBREADONDATA_MASK;
  else
    mSbcsReg &= ~SBREADONDATA_MASK;
}

/// \brief Get the \c sberror bits in \c sbcs.
///
/// \return  The value of the \c sberror bits in \c sbcs.
Dmi::Sbcs::SberrorVal
Dmi::Sbcs::sberror () const
{
  SberrorVal err
      = static_cast<SberrorVal> ((mSbcsReg & SBERROR_MASK) >> SBERROR_OFFSET);

  switch (err)
    {
    case SBERR_NONE:
    case SBERR_TIMEOUT:
    case SBERR_BAD_ADDR:
    case SBERR_ALIGNMENT:
    case SBERR_BAD_SIZE:
    case SBERR_OTHER:
      return err;

    default:
      return SBERR_UNKNOWN;
    }
}

/// \brief Clear the \c sberror bits in \c sbcs.
///
/// Writing 1 to these bits clears the error.
void
Dmi::Sbcs::sberrorClear ()
{
  mSbcsReg |= SBERROR_MASK;
}

/// \brief Get the \c sbasize bits in \c sbcs.
///
/// \return  The value of the \c sbasize bits in \c sbcs.
uint8_t
Dmi::Sbcs::sbasize () const
{
  return static_cast<uint8_t> ((mSbcsReg & SBASIZE_MASK) >> SBASIZE_OFFSET);
}

/// \brief Get the \c sbaccess128 bit in \c sbcs.
///
/// \return  \c true if the \c sbaccess128 bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbaccess128 () const
{
  return (mSbcsReg & SBACCESS128_MASK) != 0;
}

/// \brief Get the \c sbaccess64 bit in \c sbcs.
///
/// \return  \c true if the \c sbaccess64 bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbaccess64 () const
{
  return (mSbcsReg & SBACCESS64_MASK) != 0;
}

/// \brief Get the \c sbaccess32 bit in \c sbcs.
///
/// \return  \c true if the \c sbaccess32 bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbaccess32 () const
{
  return (mSbcsReg & SBACCESS32_MASK) != 0;
}

/// \brief Get the \c sbaccess16 bit in \c sbcs.
///
/// \return  \c true if the \c sbaccess16 bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbaccess16 () const
{
  return (mSbcsReg & SBACCESS16_MASK) != 0;
}

/// \brief Get the \c sbaccess8 bit in \c sbcs.
///
/// \return  \c true if the \c sbaccess8 bit of \c sbcs is set, \c false
///          otherwise.
bool
Dmi::Sbcs::sbaccess8 () const
{
  return (mSbcsReg & SBACCESS8_MASK) != 0;
}

/// \brief Give the name of a \c sbversion field.
///
/// \param[in] val  The value of the \c sbversion field
/// \return The name of the field.
const char *
Dmi::Sbcs::sbversionName (uint8_t val)
{
  switch (val)
    {
    case 0:
      return "pre 1 Jan 2019";
    case 1:
      return "debug spec 0.13.2";

    default:
      return "reserved";
    }
}

/// \brief Give the name of a \c sbaccess field.
///
/// \param[in] val  The value of the \c sbaccess field
/// \return The name of the field.
const char *
Dmi::Sbcs::sbaccessName (Dmi::Sbcs::SbaccessVal val)
{
  switch (val)
    {
    case SBACCESS_8:
      return "8-bit";
    case SBACCESS_16:
      return "16-bit";
    case SBACCESS_32:
      return "32-bit";
    case SBACCESS_64:
      return "64-bit";
    case SBACCESS_128:
      return "128-bit";

    default:
      return "??";
    }
}

/// \brief Give the name of a \c sberror field.
///
/// \param[in] val  The value of the \c sberror field
/// \return The name of the field.
const char *
Dmi::Sbcs::sberrorName (Dmi::Sbcs::SberrorVal val)
{
  switch (val)
    {
    case SBERR_NONE:
      return "none";
    case SBERR_TIMEOUT:
      return "timeout";
    case SBERR_BAD_ADDR:
      return "bad address";
    case SBERR_ALIGNMENT:
      return "bad alignment";
    case SBERR_BAD_SIZE:
      return "bad size";
    case SBERR_OTHER:
      return "other";

    default:
      return "???";
    }
}

/// \brief Output operator for the Dmi::Sbcs class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Sbcs> &p)
{
  std::ostringstream oss;

  if (p->mPrettyPrint)
    {
      uint8_t version = p->sbversion ();
      Dmi::Sbcs::SbaccessVal aval = p->sbaccess ();
      Dmi::Sbcs::SberrorVal err = p->sberror ();
      oss << "[ sbversion = " << static_cast<uint16_t> (version) << " ("
          << Dmi::Sbcs::sbversionName (version) << ")"
          << ", sbbusyerror = " << Utils::boolStr (p->sbbusyerror ())
          << ", sbbusy = " << Utils::boolStr (p->sbbusy ())
          << ", sbreadonaddr = " << Utils::boolStr (p->sbreadonaddr ())
          << ", sbaccess = " << static_cast<uint16_t> (aval) << " ("
          << Dmi::Sbcs::sbaccessName (aval) << ")"
          << ", sbautoincrement = " << Utils::boolStr (p->sbautoincrement ())
          << ", sbreadondata = " << Utils::boolStr (p->sbreadondata ())
          << ", sberror = " << static_cast<uint16_t> (err) << " ("
          << Dmi::Sbcs::sberrorName (err) << ")"
          << ", sbasize = " << static_cast<uint16_t> (p->sbasize ())
          << ", sbaccess128 = " << Utils::boolStr (p->sbaccess128 ())
          << ", sbaccess64 = " << Utils::boolStr (p->sbaccess64 ())
          << ", sbaccess32 = " << Utils::boolStr (p->sbaccess32 ())
          << ", sbaccess16 = " << Utils::boolStr (p->sbaccess16 ())
          << ", sbaccess8 = " << Utils::boolStr (p->sbaccess8 ()) << " ]";
    }
  else
    oss << Utils::hexStr (p->mSbcsReg, 8);

  return s << oss.str ();
}

/*******************************************************************************
 *                                                                             *
 * The Dmi::Sbdata class                                                       *
 *                                                                             *
 ******************************************************************************/

/// \brief constructor for the Dmi::Sbdata class
///
/// \param[in] dtm_  The DTM we shall use.  This is a unique_ptr owned by the
///                  Dmi, so passed and stored by reference.
Dmi::Sbdata::Sbdata (unique_ptr<IDtm> &dtm_) : mDtm (dtm_)
{
  for (size_t i = 0; i < NUM_REGS; i++)
    mSbdataReg[i] = 0x0;
}

/// \brief Read the value of the specified abstract \c sbdata register.
///
/// The register is refreshed via the DTM.
///
/// \param[in] n  Index of the \c sbdata register to read.
void
Dmi::Sbdata::read (const size_t n)
{
  if (n < NUM_REGS)
    mSbdataReg[n] = mDtm->dmiRead (DMI_ADDR[n]);
  else
    cerr << "Warning: reading sbdata[" << n << "] invalid: ignored." << endl;
}

/// \brief Set the specified abstract \c sbdata register to its reset value.
void
Dmi::Sbdata::reset (const size_t n)
{
  if (n < NUM_REGS)
    mSbdataReg[n] = RESET_VALUE;
  else
    cerr << "Warning: reseting sbdata[" << n << "] invalid: ignored." << endl;
}

/// \brief Write the value of the specified abstract \c sbdata register.
///
/// The register is refreshed via the DTM, and we save the value read back.
void
Dmi::Sbdata::write (const size_t n)
{
  if (n < NUM_REGS)
    mDtm->dmiWrite (DMI_ADDR[n], mSbdataReg[n]);
  else
    cerr << "Warning: writing sbdata[" << n << "] invalid: ignored." << endl;
}

/// \brief Must define as well as declare our private constexpr before using.
constexpr uint64_t Dmi::Sbdata::DMI_ADDR[Dmi::Sbdata::NUM_REGS];

/// Get the value the specified \c sbdata register.
///
/// \param[in] n  Index of the \c sbdata register to get.
/// \return  The value in the specified \c sbdata register.
uint32_t
Dmi::Sbdata::sbdata (const size_t n) const
{
  if (n < NUM_REGS)
    return mSbdataReg[n];
  else
    {
      cerr << "Warning: getting sbdata[" << n << "] invalid: zero returned."
           << endl;
      return 0;
    }
}

/// Set the value the specified \c sbdata register.
///
/// \param[in] n         Index of the \c sbdata register to get.
/// \param[in] sbdataValy  The value to be set in the specified \c sbdata
/// register.
void
Dmi::Sbdata::sbdata (const size_t n, const uint32_t sbdataVal)
{
  if (n < NUM_REGS)
    mSbdataReg[n] = sbdataVal;
  else
    cerr << "Warning: setting sbdata[" << n << "] invalid: ignored." << endl;
}

/// \brief Output operator for the Dmi::Sbdata class
///
/// \param[in] s  The stream to which output is written
/// \param[in] p  A unique_ptr to the instance to output
/// \return  The stream with the instance appended
std::ostream &
operator<< (ostream &s, const unique_ptr<Dmi::Sbdata> &p)
{
  std::ostringstream oss;

  oss << "[";
  for (size_t i = 0; i < p->NUM_REGS; i++)
    oss << "0x" << Utils::hexStr (p->mSbdataReg[i], 8)
        << ((i == (p->NUM_REGS - 1)) ? "]" : ", ");

  return s << oss.str ();
}
