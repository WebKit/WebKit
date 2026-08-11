# libpas - Phil's Awesome System

Libpas is a configurable memory allocator toolkit designed to enable adoption
of isoheaps. Currently, libpas' main client is WebKit's bmalloc project, where
it's used as a replacement for all of bmalloc's functionality (bmalloc::api,
IsoHeap<>, and Gigacage). Libpas' jit_heap API is also used by WebKit's
ExecutableAllocator.


# How To Build And Test

This section describes how to build libpas standalone. You'll be doing this a
lot when making changes to libpas. It's wise to run libpas' tests before
trying out your change in any larger system (like WebKit) since libpas tests
are great at catching bugs. If libpas passes its own tests then basic browsing
will seem to work. In production, libpas gets built as part of some other
project (like bmalloc), which just pulls all of libpas' files into that
project's build system.

Build and Test

    ./build_and_test.sh

Build

    ./build.sh

Test

    ./test.sh

Clean

    ./clean.sh

On my M1 machine, I usually do this

    ./build_and_test.sh -a arm64e

This avoids building fat arm64+arm64e binaries.

The libpas build will, by default, build (and test, if you're use
`build_and_test.sh` or `test.sh`) both a testing variant and a default variant.
The testing variant has testing-only assertions. Say you're doing some
speed tests and you just want to build the default variant:

    ./build.sh -v default

By default, libpas builds Release, but you can change that:

    ./build.sh -c Debug

All of the tools (`build.sh`, `test.sh`, `build_and_test.sh`, and `clean.sh`)
take the same options (`-h`, `-c <config>`, `-s <sdk>`, `-a <arch>`,
`-v <variant>`, `-t <target>`, and `-p <port>`).

Libpas creates multiple binaries (`test_pas` and `chaos`) during compilation, which are used by `test.sh`. Calling these binaries directly may be preferred if you would like to test or debug just one or a handful of test cases.

`test_pas` allows you to filter which test cases will be run. These are a few examples

	  ./test_pas 'JITHeapTests' # Run all JIT heap tests
	  ./test_pas 'testPGMSingleAlloc()' # Run specific test
	  ./test_pas '(1):' # Run test case 1

# Synopsis

Libpas is a toolkit for creating memory allocators. When built as a dylib, it
exposes a bunch of different memory allocators, with different configurations
ranging from sensible to deranged, without overriding malloc and friends. For
production use, libpas is meant to be built as part of some larger malloc
project; for example, when libpas sees the PAS_BMALLOC macro, it will provide
everything that WebKit's bmalloc library needs to create an allocator.

Libpas' toolkit of allocators has three building blocks:

- The segregated heap. This implements something like simple segregated
  storage. Roughly speaking, size classes hold onto collections of pages, and
  each page contains just objects of that size. The segregated heap also has
  some support for pages having multiple size classes, but this is intended as
  a "cold start" mode to reduce internal fragmentation. The segregated heap
  works great as an isoheap implementation since libpas makes it easy and
  efficient to create _lots_ of heaps, and heaps have special optimizations for
  having only one size class. Segregated heaps are also super fast. They beat
  the original bmalloc at object churn performance. A typical segregated
  allocation operation does not need to use any atomic operations or fences
  and only relies on a handful of loads, some addition and possibly bit math,
  and a couple stores. Ditto for a typical segregated deallocation operation.

- The bitfit heap. This implements first-fit allocation using bit-per-minalign-
  granule. Bitfit heaps are super space-efficient but not nearly as fast as
  segregated heaps. Bitfit heaps have an upper bound on the sizes they can
  handle, but can be configured to allocate objects up to hundreds of KB.
  Bitfit heaps are not appropriate for isoheaps. Bitfit is used mostly for
  "marge" allocations (larger than segregated's medium but smaller than large)
  and for the jit_heap.

- The large heap. This implements a cartesian-tree-based first-fit allocator
  for arbitrary sized objects. Large heaps are appropriate for isoheaps; for
  example they remember the type of every free object in memory and they
  remember where the original object boundaries are.

Each of the building blocks can be configured in a bunch of ways. Libpas uses
a C template programming style so that the segregated and bitfit heaps can be
configured for different page sizes, minaligns, security-performance
trade-offs, and so on.

All of the heaps are able to participate in physical page sharing. This means
that anytime any system page of memory managed by the heap becomes totally
empty, it becomes eligible for being returned to the OS via decommit. Libpas'
decommit strategy is particularly well tuned so as to compensate for the
inherent memory overheads of isoheaping. Libpas achieves much better memory
usage than bmalloc because it returns pages sooner than bmalloc would have, and
it does so with neglibile performance overheads. For example, libpas can be
configured to return a page to the OS anytime it has been free for 300ms.

Libpas is a heavy user of fine-grained locking and intricate lock dancing. Some
data structures will be protected by any of a collection of different locks,
and lock acquisition involves getting the lock, checking if you got the right
lock, and possibly relooping. Libpas' algorithms are designed around:

- Reducing the likelihood that any long-running operation would want to hold a
  lock that any frequently-running operation would ever need. For example,
  decommitting a page requires holding a lock and making a syscall, but the
  lock that gets held is one that has a low probability of ever being needed
  during any other operation. That "low probability" is not guaranteed but it
  is made so thanks to lots of different tricks.

- Reducing the likelihood that two operations that run in a row that both grab
  locks would need to grab different locks. Libpas has a bunch of tricks to
  make it so that data structures that get accessed together dynamically adapt
  to using the same locks to reduce the total number of lock acquisitions.

Finally, libpas is designed for having lots of tests, including unit and mock
tests. While libpas itself is written in C (to reduce friction of adoptiong it
in either C or C++ codebases), the test suite is written in C++. Ideally
everything that libpas does has a test.

