// Declaration of a class to manage a JTAG TAP simulation.
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#ifndef TAP_H
#define TAP_H

#include <sstream>

#include "VSim.h"

/// \brief Class to model a IEEE 1149.1 Test Access Port (TAP) state machine.
class Tap
{
public:
  // Constructor and destructor
  Tap (const uint64_t clkPeriodNs, const uint64_t simTimeNs,
       const char *vcdFile);
  Tap (const Tap &) = delete;
  ~Tap ();

  // API calls
  void rtiCount (const uint8_t rtiCount_);
  bool reset ();
  uint64_t accessReg (const uint8_t ir, uint64_t wdata, const uint8_t len);
  void writeReg (const uint8_t ir, uint64_t wdata, const uint8_t len);
  uint64_t readReg (const uint8_t ir, const uint8_t len);
  uint64_t simTimeNs () const;

  // Delete the copy assignment operator
  Tap &operator= (const Tap &) = delete;

private:
  /// \brief Enumeration of the TAP states.
  ///
  /// The numbering matches that used in the CV32E40P core for convenience.
  enum State
  {
    TEST_LOGIC_RESET = 0x0,
    RUN_TEST_IDLE = 0x1,
    SELECT_DR_SCAN = 0x2,
    CAPTURE_DR = 0x3,
    SHIFT_DR = 0x4,
    EXIT1_DR = 0x5,
    PAUSE_DR = 0x6,
    EXIT2_DR = 0x7,
    UPDATE_DR = 0x8,
    SELECT_IR_SCAN = 0x9,
    CAPTURE_IR = 0xa,
    SHIFT_IR = 0xb,
    EXIT1_IR = 0xc,
    PAUSE_IR = 0xd,
    EXIT2_IR = 0xe,
    UPDATE_IR = 0xf,
    NUM_STATES = UPDATE_IR + 1,
  };

  /// \brief Length of the JTAG TAP instruction register
  ///
  /// \todo Should this be hard-coded like this?
  static const std::size_t IR_LEN = 5;

  /// \brief Length of the IDCODE
  ///
  /// \todo Should this be hard-coded like this?
  static const std::size_t IDCODE_LEN = 32;

  /// \brief The Verilator simulation of the MCU associated with the JTAG TAP
  std::unique_ptr<VSim> mMcu;

  /// \brief The current state of the TAP
  State mCurrState;

  /// \brief The most recent IR shifted
  uint8_t mLastIr;

  /// \brief The number of cycles to stay in Run-Test/Idle
  ///
  /// When we access the same register multiple times, we do not need to shift
  /// the IR again, but may be required to go via Run-Test/Idle state and stay
  /// their for 1 or more cycles.  This records the count.
  uint8_t mRtiCount;

  // Helper functions/operators
  void shiftIr (const uint8_t ireg);
  uint64_t shiftDr (const uint64_t dreg, size_t len);
  bool gotoState (const State s);
  bool advanceState (const bool tms, const bool tdi);
  void nextState (const bool tms);
  const char *nameState (const State s) const;
};

#endif // TAP_H
