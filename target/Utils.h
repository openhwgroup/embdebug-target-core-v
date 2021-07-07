// Declaration of some useful static utils
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#ifndef UTILS_H
#define UTILS_H

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

/// \brief A class of static utilities
class Utils
{
public:
  // Constructor and destructor
  Utils () = default;
  ~Utils () = default;

  // Generate hex representations of numbers
  static std::string hexStr (uint8_t val, std::size_t len = 2);
  static std::string hexStr (uint16_t val, std::size_t len = 4);
  static std::string hexStr (uint32_t val, std::size_t len = 8);
  static std::string hexStr (uint64_t val, std::size_t len = 16);
  static std::string boolStr (bool flag);
  static std::string nonZero (uint64_t val);
  static std::string decStr (std::size_t val);

  // Generate a padding string
  static std::string padStr (std::string s, std::size_t w, char c = ' ');

  // Generate a random number
  static uint32_t rand (uint32_t n);

private:
  static std::ostringstream sOss;
  static std::string sPadding;
};

#endif // UTILS_H
