# This file defines useful utilities and sets up compilers for the gdbserver target

# Macro for testing whether a flag is supported, and adding it to CFLAGS/CXXFLAGS
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
macro (add_if_supported flag testname)
  check_c_compiler_flag(${flag} "C_SUPPORTS_${testname}")
  if(C_SUPPORTS_${testname})
    set(CMAKE_C_FLAGS "${flag} ${CMAKE_C_FLAGS}")
  endif()
  check_cxx_compiler_flag(${flag} "CXX_SUPPORTS_${testname}")
  if(CXX_SUPPORTS_${testname})
    set(CMAKE_CXX_FLAGS "${flag} ${CMAKE_CXX_FLAGS}")
  endif()
endmacro()

macro(configure_compiler_defaults)
  # Set warning/error flags
  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_if_supported("/W4" "WARNINGS")
    set(CMAKE_CXX_FLAGS "-D_CRT_SECURE_NO_WARNINGS ${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "-D_CRT_SECURE_NO_WARNINGS ${CMAKE_C_FLAGS}")
  else()
    add_if_supported("-Wall -Wextra -pedantic -Wno-aligned-new -Wno-format" "WARNINGS")

    # Suppress warnings triggered by the verilated code
    add_if_supported("-Wno-aligned-new" "WARNINGS")
  endif()
  if (TARGET_ENABLE_WERROR)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      add_if_supported("/WX" "WERROR")
    else()
      add_if_supported("-Werror" "WERROR")
    endif()
  endif()
endmacro()

# Helper target for formatting source
macro(enable_clang_format)
  find_program(CLANG_FORMAT "clang-format")
  if(NOT CLANG_FORMAT)
    message(STATUS "clang-format not found")
  else()
    message(STATUS "clang-format found: ${CLANG_FORMAT}. Enabling target clang-format")
    add_custom_target(clang-format
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      COMMAND find target -type f \\\( -name '*.c' -o -name '*.cpp' -o -name '*.h' \\\) -exec ${CLANG_FORMAT} -i {} \\\;
    )
  endif()
endmacro()
