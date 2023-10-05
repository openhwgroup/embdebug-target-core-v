# CV32E40 Cmake configuration
#
# This file is part of the Embecosm Debug Server targets.
#
# Copyright (C) 2020 Embecosm Limited
# SPDX-License-Identifier: MIT
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.4.3)

# Note that this will automatically set PROJECT_VERSION, PROJECT_VERSION_MAJOR,
# PROJECT_VERSION_MINOR and PROJECT_VERSION_PATCH variables.
project(cv32e40_embdebug_target VERSION 0.0.0 LANGUAGES C CXX)

# Option handling - debug server headers
if(CMAKE_PROJECT_NAME STREQUAL "embdebug")
  set (EMBDEBUG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
else()
  option(EMBDEBUG_INCLUDE_DIR "Include directory for debug server headers")
  if(NOT EMBDEBUG_INCLUDE_DIR)
    message(SEND_ERROR "No include directory specified for debug server headers")
  endif()
endif()

option(CV_MCU_BUILD_DIR "Build directory for CORE-V MCU")
  if(NOT CV_MCU_BUILD_DIR)
    message(SEND_ERROR "No include directory specified for CORE-V MCU")
  endif()

# Configure compiler

# Set C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configure compiler only if not being built as part of EmbDebug
if(NOT CMAKE_PROJECT_NAME STREQUAL "embdebug")
  option(TARGET_ENABLE_WERROR "Enable build failure if warnings triggered." OFF)
  include(cmake/compiler_utils.cmake)
  configure_compiler_defaults()

  enable_clang_format()
endif()

# Allow to find include dir whether building standalone or from within
# Embdebug.  One of these will be invalid, but that shouldn't matter.
set(CXXOPTS_INCLUDE_DIR_GLOBAL ${CMAKE_SOURCE_DIR}/targets/embdebug-target-core-v/vendor/cxxopts-3.0.0rc/include)
set(CXXOPTS_INCLUDE_DIR_LOCAL ${CMAKE_SOURCE_DIR}/vendor/cxxopts-3.0.0rc/include)

# EmbDebug target build
include_directories(${EMBDEBUG_INCLUDE_DIR})
include_directories(${CV_MCU_BUILD_DIR})
include_directories(${CXXOPTS_INCLUDE_DIR_GLOBAL})
include_directories(${CXXOPTS_INCLUDE_DIR_LOCAL})

# Get Verilator
# If it's empty, but pkg-config is available, try using that to find the
# header directory
option(VERILATOR_INCLUDE_DIR "Include directory for verilator headers")
if(NOT VERILATOR_INCLUDE_DIR)
  message(STATUS "No include directory specified for verilator headers, "
          "Checking for pkg-config to find verilator headers")
  find_program(PKG_CONFIG "pkg-config")
  if(NOT PKG_CONFIG)
    message(FATAL_ERROR "pkg-config not found, cannot find verilator headers")
  else()
    message(STATUS "pkg-config found: ${PKG_CONFIG}.")
    message(STATUS "Using pkg-config to find verilator headers")
    execute_process(COMMAND pkg-config --variable=includedir verilator
                    OUTPUT_VARIABLE VERILATOR_INCLUDE_DIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT VERILATOR_INCLUDE_DIR)
      message(FATAL_ERROR "Failed to find verilator headers with pkg-config")
    else()
      message(STATUS "Found verilator headers: ${VERILATOR_INCLUDE_DIR}")
    endif()
  endif()
endif()

include_directories(${VERILATOR_INCLUDE_DIR} ${VERILATOR_INCLUDE_DIR}/vltstd)

# What model to use
option(CV_MODEL "Verilator model type for CORE-V")
if(NOT CV_MODEL)
  message(FATAL_ERROR "No CORE-V model specified")
elseif(CV_MODEL STREQUAL "X-HEEP")
  add_compile_definitions(CV_MODEL_X_HEEP)
elseif(CV_MODEL STREQUAL "MCU")
  add_compile_definitions(CV_MODEL_MCU)
else()
  message(FATAL_ERROR "Invalid CORE-V model specified ${CV_MODEL}")
endif()

# Have to build Verilator models with -fPIC
add_compile_options("-fPIC")
add_link_options("-lz")

# If these files aren't specified as GENERATED at this level then cmake tries
# to find them at configure time before they have been generated.
set(CV32E40_EMBDEBUG_TARGET_SRCS target/Cv32e40.cpp target/Args.cpp target/DtmJtag.cpp target/Tap.cpp target/VSim.cpp target/Dmi.cpp target/Utils.cpp ${VERILATOR_INCLUDE_DIR}/verilated.cpp ${VERILATOR_INCLUDE_DIR}/verilated_vcd_c.cpp ${VERILATOR_INCLUDE_DIR}/verilated_fst_c.cpp)

add_library(embdebug-target-cv32e40 SHARED ${CV32E40_EMBDEBUG_TARGET_SRCS})
if(CV_MODEL STREQUAL "X-HEEP")
  target_link_libraries(embdebug-target-cv32e40 ${CV_MCU_BUILD_DIR}/Vtestharness__ALL.a ${CV_MCU_BUILD_DIR}/libsupport.a)
elseif(CV_MODEL STREQUAL "MCU")
  target_link_libraries(embdebug-target-cv32e40 ${CV_MCU_BUILD_DIR}/Vcore_v_mcu__ALL.a)
endif()

set_target_properties(embdebug-target-cv32e40 PROPERTIES
                      VERSION ${cv32e40_embdebug_target_VERSION}
                      SOVERSION ${cv32e40_embdebug_target_VERSION_MAJOR})

install(TARGETS embdebug-target-cv32e40
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)

message("
  " ${PROJECT_NAME} " version " ${cv32e40_embdebug_target_VERSION} "
  Prefix.......................: " ${CMAKE_INSTALL_PREFIX} "
  C++ Compiler.................: " ${CMAKE_CXX_COMPILER} "
  cxx-opts.....................: " ${CXXOPTS_INCLUDE_DIR} "
  EmbDebug include directory...: " ${EMBDEBUG_INCLUDE_DIR} "
  CORE-V model.................: " ${CV_MODEL} "
  Verilator include directory..: " ${VERILATOR_INCLUDE_DIR} "
  ")
