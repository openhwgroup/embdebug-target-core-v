// Declaration of the generic Debug Transport Module interface
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#ifndef IDTM_H
#define IDTM_H

#include <cstdint>
#include <memory>

/// \brief Abstract class for a Debug Transport Module
///
/// This must be subclassed by an actual implementation such as JTAG or USB.
class IDtm
{
public:
  // Constructor and destructor
  IDtm () = default;
  IDtm (const IDtm &) = delete;
  virtual ~IDtm () = default;

  // Core API
  virtual bool reset () = 0;
  virtual uint32_t dmiRead (uint64_t address) = 0;
  virtual void dmiWrite (uint64_t address, uint32_t wdata) = 0;
  virtual uint64_t simTimeNs () const = 0;

  // Delete the copy assignment operator
  IDtm &operator= (const IDtm &) = delete;
};

#endif // IDTM_H
