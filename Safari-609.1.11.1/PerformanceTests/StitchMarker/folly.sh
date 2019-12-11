#!/bin/bash

set -e
set -u
set -x

FLAGS="-Os -std=c++14 -fvisibility=hidden -Wno-deprecated-declarations"
INCLUDES="-I./folly/ -I./folly/double-conversion/ -I./folly/glog/src/ -I./folly/gtest/googletest/include/ -I./folly/gtest/googletest/"

DEPENDENCIES="
             ./folly/double-conversion/double-conversion/bignum-dtoa.cc
             ./folly/double-conversion/double-conversion/bignum.cc
             ./folly/double-conversion/double-conversion/cached-powers.cc
             ./folly/double-conversion/double-conversion/diy-fp.cc
             ./folly/double-conversion/double-conversion/double-conversion.cc
             ./folly/double-conversion/double-conversion/fast-dtoa.cc
             ./folly/double-conversion/double-conversion/fixed-dtoa.cc
             ./folly/double-conversion/double-conversion/strtod.cc
             ./folly/folly/Assume.cpp
             ./folly/folly/Benchmark.cpp
             ./folly/folly/Conv.cpp
             ./folly/folly/Demangle.cpp
             ./folly/folly/Format.cpp
             ./folly/folly/FormatTables.cpp
             ./folly/folly/Random.cpp
             ./folly/folly/ScopeGuard.cpp
             ./folly/folly/SharedMutex.cpp
             ./folly/folly/String.cpp
             ./folly/folly/concurrency/CacheLocality.cpp
             ./folly/folly/detail/Futex.cpp
             ./folly/folly/detail/MallocImpl.cpp
             ./folly/folly/detail/StaticSingletonManager.cpp
             ./folly/folly/detail/ThreadLocalDetail.cpp
             ./folly/folly/experimental/AsymmetricMemoryBarrier.cpp
             ./folly/folly/portability/Memory.cpp
             ./folly/folly/test/DeterministicSchedule.cpp
             ./folly/glog/src/logging.cc
             ./folly/glog/src/raw_logging.cc
             ./folly/glog/src/utilities.cc
             ./folly/glog/src/vlog_is_on.cc
"

GTEST="
      ./folly/gtest/googletest//src/gtest-filepath.cc
      ./folly/gtest/googletest/src/gtest-port.cc
      ./folly/gtest/googletest/src/gtest-printers.cc
      ./folly/gtest/googletest/src/gtest-test-part.cc
      ./folly/gtest/googletest/src/gtest.cc
"

GTEST_MAIN=./folly/gtest/googletest/src/gtest_main.cc

# concurrency/test/AtomicSharedPtrPerformance.cpp uses libstdc++ internals, ignore it.

xcrun clang++ -o CacheLocalityBenchmark ./folly/folly/concurrency/test/CacheLocalityBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o FlatCombiningBenchmark ./folly/folly/experimental/flat_combining/test/FlatCombiningBenchmark.cpp $GTEST $GTEST_MAIN $FLAGS $INCLUDES $DEPENDENCIES

xcrun clang++ -o HazptrBench-Amb-Tc ./folly/folly/experimental/hazptr/bench/HazptrBench-Amb-Tc.cpp $GTEST $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o HazptrBench-NoAmb-NoTc ./folly/folly/experimental/hazptr/bench/HazptrBench-NoAmb-NoTc.cpp $GTEST $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o HazptrBench-OneDomain ./folly/folly/experimental/hazptr/bench/HazptrBench-OneDomain.cpp $GTEST $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o HazptrBench-NoAmb-Tc ./folly/folly/experimental/hazptr/bench/HazptrBench-NoAmb-Tc.cpp $GTEST $FLAGS $INCLUDES $DEPENDENCIES

# FIXME These use urcu but I commented it out. That's a shame, we should test RCU. The rest of these tests are otherwise useful.
xcrun clang++ -o ReadMostlySharedPtrBenchmark ./folly/folly/experimental/test/ReadMostlySharedPtrBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o RefCountBenchmark ./folly/folly/experimental/test/RefCountBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES

xcrun clang++ -o BatonBenchmark ./folly/folly/test/BatonBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o CallOnceBenchmark ./folly/folly/test/CallOnceBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o ConcurrentSkipListBenchmark ./folly/folly/test/ConcurrentSkipListBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o ProducerConsumerQueueBenchmark ./folly/folly/test/ProducerConsumerQueueBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o ThreadLocalBenchmark ./folly/folly/test/ThreadLocalBenchmark.cpp $FLAGS $INCLUDES $DEPENDENCIES
