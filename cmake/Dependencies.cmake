# Prefer an explicitly supplied prefix; Homebrew is a convenience hint on macOS.
if(APPLE)
  find_program(QUANTA_BREW brew)
  if(QUANTA_BREW)
    execute_process(COMMAND "${QUANTA_BREW}" --prefix apache-arrow
      OUTPUT_VARIABLE QUANTA_ARROW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE QUANTA_BREW_RESULT)
    if(QUANTA_BREW_RESULT EQUAL 0)
      list(APPEND CMAKE_PREFIX_PATH "${QUANTA_ARROW_PREFIX}")
    endif()
  endif()
endif()
# QUIET propagates to the packages' optional config probes; REQUIRED still fails
# visibly when discovery cannot find a dependency through any supported mechanism.
find_package(Arrow CONFIG QUIET REQUIRED)
find_package(Parquet CONFIG QUIET REQUIRED)
message(STATUS "Quanta storage: Arrow ${ARROW_VERSION}, Parquet ${Parquet_VERSION}")
if(ARROW_VERSION VERSION_LESS 23 OR Parquet_VERSION VERSION_LESS 23)
  message(FATAL_ERROR "Quanta requires Arrow and Parquet 23 or newer")
endif()
foreach(target Arrow::arrow_shared Parquet::parquet_shared)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Installed package must provide ${target}; install shared Arrow/Parquet development libraries")
  endif()
endforeach()
include(FetchContent)
if(QUANTA_BUILD_TESTS)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  FetchContent_MakeAvailable(googletest)
endif()
if(QUANTA_BUILD_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
    URL_HASH SHA256=9631341c82bac4a288bef951f8b26b41f69021794184ece969f8473977eaa340
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  FetchContent_MakeAvailable(benchmark)
endif()
