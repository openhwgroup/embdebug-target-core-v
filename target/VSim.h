// Declaration of a class to manage a Verilator simulation.
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#ifndef VSIM_H
#define VSIM_H

#include "verilated.h"
#include <verilated_vcd_c.h>

#include "Vcore_v_mcu.h"

/// \brief A class to wrap a Verilator simulation of a processor.
///
/// We hide the clocking from the user.
class VSim
{
public:
  // Constructors and destructors
  VSim (const vluint64_t clkPeriodNs, const vluint64_t simTimeNs,
        const char *vcdFile);
  VSim (const VSim &) = delete;
  ~VSim ();

  // Prior to Verilator 4.200, we need to provide access to a timestamp
#if (VERILATOR_MAJOR < 4) || ((VERILATOR_MAJOR == 4) && (VERILATOR_MINOR < 200))
  double sc_time_stamp ();
#endif

  // API
  vluint64_t simTimeNs () const;
  bool allDone () const;
  void advanceHalfPeriod ();
  bool inReset () const;
  bool tapPosedge () const;
  bool tapNegedge () const;
  void eval ();

  // Port accessors
  void tdi (const bool tdi_);
  bool tdi () const;
  bool tdo () const;
  void tms (const bool tms_);
  bool tms () const;

  // Delete the copy assignment operator
  VSim &operator= (const VSim &) = delete;

private:
  /// \brief The verilator simulation context
  std::unique_ptr<VerilatedContext> mContextp;

  /// \brief Is dumping requested?
  bool mHaveVcd;

  /// \brief Verilator dump state
  std::unique_ptr<VerilatedVcdC> mTfp;

  /// \brief Verilator model
  std::unique_ptr<Vcore_v_mcu> mCpu;

  /// \brief Half period of the main clock in ticks
  vluint64_t mClkHalfPeriodTicks;

  /// \brief Half period of the JTAG clock in ticks
  vluint64_t mTckHalfPeriodTicks;

  /// \brief Reset period in ticks
  vluint64_t mResetPeriodTicks;

  /// \brief Sim time in ticks
  vluint64_t mSimTimeTicks;

  /// \brief How many ticks we have executed so far
  vluint64_t mTickCount;

  /// \brief Are we at a TAP clock posedge
  bool mTckPosedge;

  /// \brief Are we at a TAP clock negedge
  bool mTckNegedge;
};

#endif // VSIM_H
