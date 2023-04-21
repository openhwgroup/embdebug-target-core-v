// Definition of some useful static utils
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Utils.h"

using std::dec;
using std::hex;
using std::ostringstream;
using std::setfill;
using std::setw;
using std::size_t;
using std::string;

/// Instantiate the output string stream.
ostringstream Utils::sOss;

/// Instatiate the padding string
string Utils::sPadding;

/// \brief Generate the hexadecimal representation of an octet.
///
/// \note No leading "0x".
///
/// \param[in] val  The value to print.
/// \param[in] len  The number of digits to print (default 2).
/// \return  A string with the hexadecimal representation of \p val, left
///          padded with zeros to at least \p len chars.
string
Utils::hexStr (const uint8_t val, const size_t len)
{
  sOss.str ("");
  // Cast is because 8 bit numerical values always attempt to print as a
  // character.
  sOss << hex << setfill ('0') << setw (len) << static_cast<uint16_t> (val);
  return sOss.str ();
}

/// \brief Generate the hexadecimal representation of a 16-bit word.
///
/// \note No leading "0x".
///
/// \param[in] val  The value to print.
/// \param[in] len  The number of digits to print (default 4).
/// \return  A string with the hexadecimal representation of \p val, left
///          padded with zeros to at least \p len chars.
string
Utils::hexStr (const uint16_t val, const size_t len)
{
  sOss.str ("");
  sOss << hex << setfill ('0') << setw (len) << val;
  return sOss.str ();
}

/// \brief Generate the hexadecimal representation of a 32-bit word.
///
/// \note No leading "0x".
///
/// \param[in] val  The value to print.
/// \param[in] len  The number of digits to print (default 8).
/// \return  A string with the hexadecimal representation of \p val, left
///          padded with zeros to at least \p len chars.
string
Utils::hexStr (const uint32_t val, const size_t len)
{
  sOss.str ("");
  sOss << hex << setfill ('0') << setw (len) << val;
  return sOss.str ();
}

/// \brief Generate the hexadecimal representation of a 64-bit word.
///
/// \note No leading "0x".
///
/// \param[in] val  The value to print.
/// \param[in] len  The number of digits to print (default 16).
/// \return  A string with the hexadecimal representation of \p val, left
///          padded with zeros to at least \p len chars.
string
Utils::hexStr (const uint64_t val, const size_t len)
{
  sOss.str ("");
  sOss << hex << setfill ('0') << setw (len) << val;
  return sOss.str ();
}

/// \brief String representation of booleans.
///
/// \param[in] flag  Value to represent.
/// \return \c "true if \p flag is true, \c "false" otherwise.
string
Utils::boolStr (bool flag)
{
  sOss.str ("");

  if (flag)
    sOss << "true";
  else
    sOss << "false";

  return sOss.str ();
}

/// \brief String representation of numbers as boolean values.
///
/// \param[in] val  Value to represent.
/// \return  \c "true" if \p val is non-zero, \c "false" otherwise.
string
Utils::nonZero (uint64_t val)
{
  sOss.str ("");

  if (val != 0)
    sOss << "true";
  else
    sOss << "false";

  return sOss.str ();
}

/// \brief Generate a string representation of an unsigned number in decimal
///
/// Useful when string versions of numbers need manipulation
///
/// \brief val  The number to represent.
/// \return A string representation of \p val.
string
Utils::decStr (size_t val)
{
  sOss.str ("");
  sOss << dec << val;
  return sOss.str ();
}

/// \brief Generate a string to pad to a given length
///
/// \param[in] s  String to pad.
/// \param[in] w  Width to which to pad.
/// \param[in] c  Character with which to pad (default space char).
/// \return The string of padding.
string
Utils::padStr (string s, size_t w, char c)
{
  size_t sLen = s.size ();

  sPadding = "";
  if (w > sLen)
    sPadding.insert (sPadding.begin (), w - sLen, c);

  return sPadding;
}

/// \brief Wrapper for random number function
///
/// Returns a uint32_t in the range [0,n)
///
/// \param[in] n  Upper end of the range
/// \returns a random number between 0 and n-1.
uint32_t
Utils::rand (uint32_t n)
{
  return static_cast<uint32_t> (std::rand ()) % n;
}
