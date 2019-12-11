#!/bin/bash

set -e
set -u
set -x

FLAGS="-Os -W -std=c++14 -fvisibility=hidden -DWTF_DEFAULT_EVENT_LOOP=0 -DUSE_GENERIC_EVENT_LOOP=1"
INCLUDES="-I./ -I./wtf/dependencies -I./wtf/dependencies/icu/"

DEPENDENCIES="
            ./wtf/Assertions.cpp
            ./wtf/ClockType.cpp
            ./wtf/CurrentTime.cpp
            ./wtf/DataLog.cpp
            ./wtf/DateMath.cpp
            ./wtf/FastMalloc.cpp
            ./wtf/FilePrintStream.cpp
            ./wtf/FunctionDispatcher.cpp
            ./wtf/Lock.cpp
            ./wtf/LockedPrintStream.cpp
            ./wtf/MainThread.cpp
            ./wtf/MonotonicTime.cpp
            ./wtf/ParkingLot.cpp
            ./wtf/PrintStream.cpp
            ./wtf/RunLoop.cpp
            ./wtf/Seconds.cpp
            ./wtf/StackBounds.cpp
            ./wtf/StackTrace.cpp
            ./wtf/ThreadHolder.cpp
            ./wtf/ThreadHolderPthreads.cpp
            ./wtf/Threading.cpp
            ./wtf/ThreadingPthreads.cpp
            ./wtf/TimeWithDynamicClockType.cpp
            ./wtf/WTFThreadData.cpp
            ./wtf/WallTime.cpp
            ./wtf/WordLock.cpp
            ./wtf/dependencies/bmalloc/Allocator.cpp
            ./wtf/dependencies/bmalloc/Cache.cpp
            ./wtf/dependencies/bmalloc/Deallocator.cpp
            ./wtf/dependencies/bmalloc/DebugHeap.cpp
            ./wtf/dependencies/bmalloc/Environment.cpp
            ./wtf/dependencies/bmalloc/Gigacage.cpp
            ./wtf/dependencies/bmalloc/Heap.cpp
            ./wtf/dependencies/bmalloc/LargeMap.cpp
            ./wtf/dependencies/bmalloc/Logging.cpp
            ./wtf/dependencies/bmalloc/ObjectType.cpp
            ./wtf/dependencies/bmalloc/Scavenger.cpp
            ./wtf/dependencies/bmalloc/StaticMutex.cpp
            ./wtf/dependencies/bmalloc/VMHeap.cpp
            ./wtf/dependencies/bmalloc/Zone.cpp
            ./wtf/dependencies/unicode.cpp
            ./wtf/dtoa.cpp
            ./wtf/dtoa/bignum-dtoa.cc
            ./wtf/dtoa/bignum.cc
            ./wtf/dtoa/cached-powers.cc
            ./wtf/dtoa/diy-fp.cc
            ./wtf/dtoa/double-conversion.cc
            ./wtf/dtoa/fast-dtoa.cc
            ./wtf/dtoa/fixed-dtoa.cc
            ./wtf/dtoa/strtod.cc
            ./wtf/generic/MainThreadGeneric.cpp
            ./wtf/generic/RunLoopGeneric.cpp
            ./wtf/text/AtomicString.cpp
            ./wtf/text/AtomicStringImpl.cpp
            ./wtf/text/AtomicStringTable.cpp
            ./wtf/text/CString.cpp
            ./wtf/text/StringBuilder.cpp
            ./wtf/text/StringImpl.cpp
            ./wtf/text/StringView.cpp
            ./wtf/text/SymbolImpl.cpp
            ./wtf/text/SymbolRegistry.cpp
            ./wtf/text/TextBreakIterator.cpp
            ./wtf/text/WTFString.cpp
            ./wtf/text/icu/UTextProvider.cpp
            ./wtf/text/icu/UTextProviderLatin1.cpp
            ./wtf/text/icu/UTextProviderUTF16.cpp
            ./wtf/text/unix/TextBreakIteratorInternalICUUnix.cpp
            ./wtf/unicode/UTF8.cpp
"

xcrun clang++ -o ConditionSpeedTest ./wtf/benchmarks/ConditionSpeedTest.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o LockFairnessTest ./wtf/benchmarks/LockFairnessTest.cpp $FLAGS $INCLUDES $DEPENDENCIES
xcrun clang++ -o LockSpeedTest ./wtf/benchmarks/LockSpeedTest.cpp $FLAGS $INCLUDES $DEPENDENCIES
