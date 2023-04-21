// Definition of a class to process command line arguments.
//
// This file is part of the Embecosm Debug Server target for CORE-V MCU
//
// Copyright (C) 2021 Embecosm Limited
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <string>
#include <vector>

#include "Args.h"
#include "cxxopts.hpp"

using cxxopts::OptionException;
using cxxopts::Options;
using cxxopts::ParseResult;
using cxxopts::value;
using std::cerr;
using std::cout;
using std::endl;
using std::size_t;
using std::string;
using std::vector;

/// \brief Constructor.
///
/// Parse the arguments to create all the information needed.
///
/// \param [in] argc  Number of command line arguments.
/// \param [in] argv  Vector of the command line arguments.
Args::Args (int argc, char *argv[])
{
  Options options ("embdebug-target-core-v", "Embdebug CORE-V target library");
  options.add_options () ("s,mhz", "Clock speed in MHz",
                          value<double> ()->default_value ("100"), "<speed>");
  options.add_options () ("d,duration-ns", "Simulation duration in nanoseconds",
                          value<uint64_t> ()->default_value ("0"), "<time>");
  options.add_options () ("seed", "Random number seed",
                          value<unsigned int> ()->default_value ("1"), "<n>");
  options.add_options () ("max-block", "Maximum size of memory block to test",
                          value<size_t> ()->default_value ("64"), "<n>");
  options.add_options () ("vcd", "Verilog Change Dump file name",
                          value<string> ()->default_value (""), "<filename>");
  options.add_options () ("test-status", "Run a test of hart status");
  options.add_options () ("test-gprs", "Run a test of the GPRs");
  options.add_options () ("test-fprs", "Run a test of the FPRs and FPU CSRs");
  options.add_options () ("test-csrs", "Run a test of the CSRs");
  options.add_options () ("test-mem", "Run a test of memory");
  options.add_options () ("h,help", "Produce help message and exit");
  options.add_options () ("v,version", "Produce version message and exit");

  ParseResult res;
  try
    {
      res = options.parse (argc, argv);
    }
  catch (OptionException &e)
    {
      cerr << "ERROR: unable to parse arguments:" << e.what () << endl;
      cerr << options.help ();
      exit (EXIT_FAILURE);
    }

  if (res.count ("help") > 0)
    {
      cout << options.help ();
      exit (EXIT_SUCCESS);
    }

  if (res.count ("version") > 0)
    {
      cout << "embdebug-target-core-v version 0.0.0" << endl;
      exit (EXIT_SUCCESS);
    }

  double mhzVal = res["mhz"].as<double> ();

  if (mhzVal > 500.0)
    {
      cerr << "ERROR: speed cannot be greater than 500MHz" << endl;
      exit (EXIT_FAILURE);
    }

  mClkPeriodNs = static_cast<uint64_t> (1000.0 / mhzVal);

  mDurationNs = res["duration-ns"].as<uint64_t> ();
  mSeed = res["seed"].as<unsigned int> ();

  mMaxBlock = res["max-block"].as<size_t> ();
  if (mMaxBlock < 1)
    mMaxBlock = 1;

  mVcd = res["vcd"].as<string> ();

  // If the filename does not end in .vcd or .VCD, then add the suffix.
  if (!mVcd.empty ())
    {
      size_t len = mVcd.size ();
      if ((len <= 4)
          || ((mVcd.rfind (".vcd", len - 4) == string::npos)
              && (mVcd.rfind (".VCD", len - 4) == string::npos)))
        mVcd.append (".vcd");
    }

  mTestStatus = res.count ("test-status") > 0;
  mTestGprs = res.count ("test-gprs") > 0;
  mTestFprs = res.count ("test-fprs") > 0;
  mTestCsrs = res.count ("test-csrs") > 0;
  mTestMem = res.count ("test-mem") > 0;
}

/// \brief Destructor.
///
/// For now nothing to do.
Args::~Args ()
{
}

/// \brief Getter for any VCD filename.
///
/// \return The filename for any VCD, the empty string if none were specified.
std::string
Args::vcd () const
{
  return mVcd;
}

/// \brief Getter for the clock period in nanoseconds.
///
/// \return The clock period in nanoseconds.
uint64_t
Args::clkPeriodNs () const
{
  return mClkPeriodNs;
}

/// \brief Getter for the random number seed.
///
/// \return The random number seed.
unsigned int
Args::seed () const
{
  return mSeed;
}

/// \brief Getter for the maximum block size.
///
/// \return The random number seed.
size_t
Args::maxBlock () const
{
  return mMaxBlock;
}

/// \brief Getter for the run duration in nanoseconds.
///
/// \return The duration in nanoseconds.
uint64_t
Args::durationNs () const
{
  return mDurationNs;
}

/// \brief Getter for whether to test hart status
///
/// \return \c true if we should test hart status, \c false otherwise.
bool
Args::testStatus () const
{
  return mTestStatus;
}

/// \brief Getter for whether to test GPRs
///
/// \return \c true if we should test GPRs, \c false otherwise.
bool
Args::testGprs () const
{
  return mTestGprs;
}

/// \brief Getter for whether to test FPRs
///
/// \return \c true if we should test FPRs, \c false otherwise.
bool
Args::testFprs () const
{
  return mTestFprs;
}

/// \brief Getter for whether to test CSRs
///
/// \return \c true if we should test CSRs, \c false otherwise.
bool
Args::testCsrs () const
{
  return mTestCsrs;
}

/// \brief Getter for whether to test memory
///
/// \return \c true if we should test memory, \c false otherwise.
bool
Args::testMem () const
{
  return mTestMem;
}
