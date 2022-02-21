# All About Libpas, Phil's Super Fast Malloc

Filip Pizlo, Apple Inc., February 2022

# License

Copyright (c) 2022 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Introduction

This document describes how libpas works as of a361efa96ca4b2ff6bdfc28bc7eb1a678cde75be, so a bit ahead of
where WebKit was as of r289146. Libpas is a fast and memory-efficient memory allocation toolkit capable of
supporting many heaps at once, engineered with the hopes that someday it'll be used for comprehensive isoheaping
of all malloc/new callsites in C/C++ programs.

Since WebKit r213753, we've been steadily enabling libpas as a replacement for WebKit's bmalloc and
MetaAllocator. This has so far added up to a ~2% Speedometer2 speed-up and a ~8% memory improvement (on multiple
memory benchmarks). Half of the speed-up comes from replacing the MetaAllocator, which was JavaScriptCore's old
way of managing executable memory. Now, JSC uses libpas's jit_heap to manage executable memory. The other half
of the speed-up comes from replacing everything that bmalloc provided -- the fastMalloc API, the Gigacage API,
and the IsoHeap<> API. All of the memory improvement comes from replacing bmalloc (the MetaAllocator was already
fairly memory-efficient).

This document is structured as follows. First I describe the goals of libpas; these are the things that a
malloc-like API created out of libpas should be able to expose as fast and memory-efficient functions. Then I
describe the coding style. Next I tell all about the design. Finally I talk about how libpas is tested.

# Goals of Libpas

Libpas tries to be:

- Fast. The goal is to beat bmalloc performance on single-threaded code. Bmalloc was previously the fastest
  known malloc for WebKit.
- Scalable. Libpas is meant to scale well on multi-core devices.
- Memory-efficient. The goal is to beat bmalloc memory usage across the board. Part of the strategy for memory
  efficiency is consistent use of first-fit allocation.
- External metadata. Libpas never puts information about a free object inside that object. The metadata is
  always elsewhere. So, there's no way for a use-after-free to corrupt libpas's understanding of memory.
- Multiple heap configurations. Not all programs want the same time-memory trade-off. Some malloc users have
  very bizarre requirements, like what JavaScriptCore does with its ExecutableAllocator. The goal is to support
  all kinds of special allocator needs simultaneously in one library.
- Boatloads of heaps. Libpas was written with the dream of obviating the need for ownership type systems or
  other compiler approaches to fixing the type-safety of use-after-frees. This means that we need one heap per
  type, and be 100% strict about it. So, libpas supports tons of heaps.
- Type-awareness. Sometimes, malloc decisions require knowing what the type's size and alignment are, like when
  deciding how to split and coalesce memory. Libpas is designed to avoid type errors arising from the malloc's
  "skewed" reuse of memory.
- Common free. Users of libpas isoheaps don't have to know the heap of an object when they free it. All objects
  should funnel into the same free function. One kind of exception to this requirement is stuff like
  ExecutableAllocator, which needs a malloc, but is fine with not calling a common free function.
- Cages. WebKit uses virtual memory reservations called *cages*, in which case WebKit allocates the virtual
  memory and the malloc has to associate that memory with some heap. Libpas supports multiple kinds of cages.

# Libpas Style

Libpas is written in C. Ultimately, I chose C because I felt that the language provides better support for
extremely low-level code:

- C++ is usually my favorite, because it makes code easier to write, but for libpas, I wanted something easier
  to read. It's easier to read C when auditing for subtle bugs, because there's nothing hidden. C doesn't have
  stuff like destructor invocations or operator overloading, which result in surprising effectfulness in
  otherwise innocent-looking code. Memory management code like libpas has to be read a lot, so C is better.
- C makes casting between pointers and integers very simple with its style of cast operator. It feels weird to
  use the C cast operator in C++, so when I have to do a lot of uintptr_t'ing, I prefer C.

C lets you do most of what C++ can if you rely on `always_inline`. This didn't used to be the case, but modern C
compilers will meat-grind the code with repeated application of the following things:

- Inlining any `always_inline` call except if it's recursive or the function uses some very weird features that
  libpas doesn't use (like goto pointer).
- Copy-propagating the values from the callsite into the function that uses the value.

Consequently, passing a function pointer (or struct of function pointers), where the pointer points to an
`always_inline` function and the callee is `always_inline` results in specialization akin to template
monomorphization. This works to any depth; the compiler won't be satisfied until there are no more
`always_inline` function calls. This fortuitous development in compilers allowed me to write very nice template
code in C. Libpas achieves templates in C using config structs that contain function pointers -- sometimes to
`always_inline` functions (when we want specialization and inlining) and sometimes to out-of-line functions
(when we want specialization but not inlining). Additionally, the C template style allows us to have true
polymorphic functions. Lots of libpas slow paths are huge and not at all hot. We don't want that code
specialized for every config. Luckily, this works just fine in C templates -- those polymorphic functions just
pass around a pointer to the config they are using, and dynamically load and call things in that config, almost
exactly the same way that the specialized code would do. This saves a lot of code size versus C++ templates.

Most of libpas is written in an object-oriented style. Structs are used to create either by-value objects or
heap-allocated objects. It's useful to think of these as classes, but a loose way since there are many ways to
do classes in C, and libpas uses whatever techniques are best on a per-class basis. But heap allocated objects
have a clear convention: for a class named `foo`, we would call the struct `pas_foo`, and for a method `bar` on
`foo`, we would call the function `pas_foo_bar` and have the first parameter be `pas_foo*`. The function that
creates instances of `foo` is called `pas_foo_create` (or `pas_foo_create_blah_blah` in case of overloading) and
returns a `pas_foo*`. The function that destroys `foo` objects is called `pas_foo_destroy` and takes a
`pas_foo*`. 

Libpas classes are usually implemented in files called `pas_foo.h` (the header that defines the struct and a
subset of functions), `pas_foo_inlines.h` (the header that defines inline functions of `foo` that require
calling functions declared in headers that `pas_foo.h` can't include), and `pas_foo.c` (the implementations of
`foo` functions that can be out-of-line).

Some libpas "classes" are singletons. The standard way of implementing a singleton in libpas is that there is
really no struct, only global variables and functions that are declared in the header. See `pas_page_malloc` or
`pas_debug_heap` for examples of singletons.

Not everything in libpas is a class. In cases where a bunch of not-class-like things can be grouped together in
a way that makes sense, we usually do something like a singleton. In cases where a function can't easily be
grouped together with some class, even a singleton, we name the file it's in after the function. There are lots
of examples of this, like `pas_deallocate` or `pas_get_page_base`. Sometimes this gets fun, like
`pas_get_page_base_and_kind_for_small_other_in_fast_megapage.h`.

Finally, libpas avoids abbreviations even more so than WebKit usually does. Functions that have a quirky
meaning typically have a long name that tells the story. The point is to make it easy to appreciate the subtlety
of the algorithm when reading the code. This is the kind of code where complex situations should look complex
at any abstraction level.

# Design of Libpas

Libpas is organized into roughly eleven areas:

1.  Heap configurations. This is the way that we tell libpas how to organize a heap. Heap configurations can
    control a lot. They can change obvious things like the minalign and page size, but also more crazy things,
    like how to find a page header given a page and vice-versa.
2.  The large heaps. This is a first-fit heap based on arrays, cartesian trees, and hashtables. The large
    heap has excellent type safety support and can be safely (though not efficiently) used for small objects.
3.  Metacircularity. Libpas uses malloc-like APIs internally for managing its state. These are provided by the
    so-called bootstrap, utility, and immortal heaps.
4.  The segregated heaps and TLCs (thread-local caches). Libpas has a super fast simple segregated storage slab
    allocator. It supports type safety and is the most commonly used kind of heap.
5.  The bitfit heaps. This is a fast and memory-efficient type-unsafe heap based on slabs and bitmaps.
6.  The scavenger. Libpas performs a bunch of periodic bookkeeping tasks in a scavenger thread. This includes,
    but is not limited to, returning memory to the OS.
7.  Megapages and page header tables. Libpas has multiple clever tricks for rapidly identifying which kind of
    heap an object belongs to. This includes an arithmetic hack called megapages and some lock-free hashtables.
8.  The enumerator. Libpas supports malloc heap enumeration APIs.
9.  The basic configuration template, used to create the `bmalloc_heap` API that is used as a replacement for
    all of bmalloc's functionality.
10. The JIT heap config.
11. The fast paths. The various heaps, TLCs, megapages and page header tables are glued together by fast paths
    provided for allocation, deallocation, and various utility functions.

## Heap Configurations

The `pas_heap_config` struct defines all of the configurable behaviors of a libpas heap. This includes things
like how the heap gets its memory, what size classes use segregated, bitfit, or large allocators, and a bunch
of other things.

Heap configs are passed by-value to functions that are meant to be specialized and inlined. To support this,
the convention for defining a heap config is that you create a macro (like `BMALLOC_HEAP_CONFIG`) that gives a
heap config literal expression. So, a call like `pas_get_allocation_size(ptr, BMALLOC_HEAP_CONFIG)` will give
you an optimized fast path for getting the allocation size of objects in bmalloc. This works because such fast
paths are `always_inline`.

Heap configs are passed by-pointer to functions that are not meant to be specialized. To support this, all
heap configs also have a global variable like `bmalloc_heap_config`, so we can do things like
`pas_large_heap_try_deallocate(base, &bmalloc_heap_config)`.

Heap configs can have up to two segregated page configs (`config.small_segregated_config` and
`config.medium_segregated_config`) and up to three bitfit page configs (`config.small_bitfit_config`,
`config.medium_bitfit_config`, and `config.marge_bitfit_config`). Any of the page configs can be disabled,
though weird things might happen if the smallest ones are disabled (rather than disabling the bigger ones).
Page configs (`pas_segregated_page_config`, `pas_bitfit_page_config`, and the common supertype,
`pas_page_base_config`) get used in much the same way as heap configs -- either by-value for specialized and
inlined functions or by-pointer for unspecialized functions.

Heap and page configs also support specialized-but-not-inlined functions. These are supported using additional
function pointers in those configs that are filled in using macros -- so you don't need to fill them in
explicitly when creating your own config, like `BMALLOC_HEAP_CONFIG` or `JIT_HEAP_CONFIG`. The macros fill them
in to point at never_inline functions that call some specialized and inlined function with the config passed as
a constant. This means for example that:

    BMALLOC_HEAP_CONFIG.specialized_local_allocator_try_allocate_small_segregated_slow(...);

Is an out-of-line direct function call to the specialization of
`pas_local_allocator_try_allocate_small_segregated_slow`. And this would be a virtual call to the same
function:

    pas_heap_config* config = ...;
    config->specialized_local_allocator_try_allocate_small_segregated_slow(...);

Note that in many cases where you have a `pas_heap_config`, you are in specialized code and the heap config is
a known constant at compile to, so then:

    config.specialized_local_allocator_try_allocate_small_segregated_slow(...);

Is an out-of-line direct function call.

## The Large Heaps

Libpas's large heaps serve multiple purposes:

- Everything is bootstrapped on large heaps. When segregated and bitfit heaps allocate memory, they do so from
  some large heap.
- Segregated and bitfit heaps have object size ceilings in the tens or hundreds of kilobytes. So, objects that
  are too large for the other heaps get allocated from large heaps.

Large heaps are broken into two parts:

1. The large free heap. In libpas jargon, a *free heap* is a heap that requires that deallocation passes the
   object size, requires that the freed object size matches the allocated object size for that object, and makes
   no guarantees about what kind of mess you'll get yourself into if you fail to obey that rule.
2. The large map. This maps object pointer to size and heap.
3. The large heap. This is an abstraction over both (1) and (2).

Large free heaps just maintain a free-list; they know nothing about allocated objects. But combined with the
large map, the large heaps provide a user-friendly deallocation API: you just need the object pointer, and the
large map figures out the rest, including identifying which free heap the object should be deallocated into, and
the size to pass to that free heap.

Large heaps operate under a single global lock, called the *heap lock*. Most libpas heaps use fine-grained
locking or avoid locking entirely. But for the large heap, libpas currently just uses one lock.

### Large Free Heap

Large free heaps are built out of a generic algorithm that doesn't know how to represent the free-list and gets
instantiated with either of two free-list representations, *simple* and *fast*. The simple large free heap uses
an unordered array of coalesced free object descriptions. The fast large free heap uses a cartesian tree of
coalesced free object descriptions.

A free object description is represented by the `pas_large_free` object in libpas; let's just call it a *large
free* for brevity. Large frees can tell you the beginning and end of a free chunk of memory. They can also tell
if the memory is already known to be zero and what the *type skew* of the free memory is. The large heap can be
used to manage arrays of some type that is either larger than the heap's minimum alignment, or that is smaller
than and not a divisor of the alignment. Especially when this is combined with `memalign`, the free heap will
have to track free memory that isn't type-aligned. Just consider a type of size 1000 that is allocated with
alignment 16384. The rules of memalign say that the size must be 16384 in that case. Assuming that the free heap
had 32768 contiguous bytes of free memory to begin with, it will now have 16384 bytes that starts with a type
skew of 384 bytes. The type skew, or `offset_in_type` as libpas calls it, is the offset of the beginning of the
large free inside the heap's type. In extremely complex cases, this means that finding where the first valid
object address is inside a large free for some type and alignment requires computing the offset least common
multiple (see `pas_coalign`), which relies on the right bezout coefficient of the extended GCD of the type size
and alignment (see `pas_extended_gcd`).

Large frees support an API for coalescing (merging as libpas calls it) and splitting. The generic large free
heap handles searching through large frees to find the first one that matches an allocation request for some
size and alignment. It also handles coalescing freed memory back into the heap, by searching for adjacent free
memory. The searches are through a struct of function pointers that may be implemented either efficiently (like
the simple large free heap's O(n) search through an unordered array) or efficiently (like the O(1)-ish or
O(log n)-ish operations on the cartesian tree in the fast large free heap). The generic algorithm uses the C
generic idiom so there are no actual function pointer calls at runtime.

Large free heaps allow you to give them callbacks for allocating and deallocating memory. The allocation
callback will get called if you ask a large free heap for memory and it doesn't have it. That allocation
callback could get the memory from the OS, or it could get it from some other heap. The deallocation callback
is for those cases where the large free heap called the allocation callback and then decided it wanted to give
some fraction of that memory back. Both callbacks are optional (can be NULL), though the case of a NULL
allocation callback and non-NULL deallocation callback is not useful since the deallocation callback only gets
called on the path where we had an allocation callback.

Note that large free heaps do not do anything to decommit their free memory. All decommit of memory in large
free heaps is accomplished by the *large sharing pool*, which is part of the scavenger.

### Large Map

The large map is a hashtable that maps object addresses to *large entries*, which contain the size of the object
and the heap it belongs to. The large map has a fairly complex hashtable algorithm because of my past attempts
at making the large heap at least somewhat efficient even for small objects. But it's conceptually a simple part
of the overall algorithm. It's also legal to ask the large map about objects it doesn't know about, in which
case, like a normal hashtable, it will just tell you that it doesn't know about your object. Combined with the
way that segregated and bitfit heaps use megapage tables and page header tables, this means that libpas can do
fall-through to another malloc for objects that libpas doesn't manage.

Note that it might be OK to remove the small object optimizations in the large map. On the other hand, they are
reliable, and they aren't known to increase the cost of the algorithm. Having that capability means that as part
of tuning the algorithm, it's more safe than it would otherwise be to try putting some small objects into the
large heap to avoid allocating the data structures required for operating a segregated or bitfit heap.

### The Large Heap

The large free heap and large map are combined into a high-level API with the `pas_large_heap`. In terms of
state, this is just a
`pas_large_free_heap` plus some data to help with small-object optimizations in the large map. The functions
of the large heap do a lot of additional work:

- They give the free heap an allocator for getting new memory. The large heap routes memory allocation
  requests to the heap config's allocation callback.
- They ensure that each free heap allocation ends up in the large map.
- They implement deallocation by removing something from the large map and then deallocating it into the free
  heap.
- They provide integration with the scavenger's large sharing pool so that free memory can be decommitted.

The large heap is always used as a member of the `pas_heap` object. It's useful to think of `pas_large_heap` as
never being a distinct object; it's more of a way of compartmentalizing `pas_heap`. The heap object also
contains a segregated heap and some other stuff.

## Metacircularity

I'm used to programming with dynamically allocated objects. This lets me build arrays, trees, hashtables,
look-up tables, and all kinds of lock-free data structures. So, I wanted libpas's internals to be able to
allocate objects just like any other kind of algorithm would do. But libpas is engineered so that it can
be a "bottom of the world" malloc -- where it is the implementation of `malloc` and `free` and cannot rely on
any memory allocation primitives other than what the kernel provides. So, libpas uses its own allocation
primitives for its own objects that it uses to implement those primitives. This is bootstrapped as follows:

- The *bootstrap heap* is a simple large free heap. A simple large free heap needs to be able to allocate
  exactly one variable-length array of large frees. The bootstrap heap has hacks to allow itself to allocate
  that array out of itself. This trick then gives us a complete malloc implementation for internal use by
  libpas, albeit one that is quite slow, can only be used under the heap lock, and requires us to know the
  object's size when freeing it. All other simple large free heaps allocate their free lists from the bootstrap
  heap. The bootstrap heap is the only heap in libpas that asks the OS for memory. All other heaps ask either
  the bootstrap heap for memory, or they ask one of the other heaps.
- The *compact reservation* is 128MB of memory that libpas uses for objects that can be pointed at with 24-bit
  (3 byte) pointers assuming 8-byte alignment. Libpas needs to manage a lot of heaps, and that requires a lot
  of internal meta-data, and having compact pointers reduces the cost of doing this. The compact reservation is
  allocated from the bootstrap heap.
- The *immortal heap* is a heap that bump-allocates out of the compact reservation. It's intended for small
  objects that are immortal.
- The *compact bootstrap heap* is like the bootstrap heap, except that it allocates its memory from the compact
  reservation, and allocates its free list from the bootstrap heap rather than itself.
- The *compact large utility free heap* is a fast large free heap that supports decommitting free memory (see
  the scavenger section) and allocates its memory from the compact bootstrap heap.
- The *utility heap* is a segregated heap configured to be as simple as possible (no thread-local caches for
  example) and can only be used while holding the heap lock. It only supports objects up to some size
  (`PAS_UTILITY_LOOKUP_SIZE_UPPER_BOUND`), supports decommitting free memory, and gets its memory from the
  compact bootstrap heap. One example of how the utility heap gets used is the nodes in the cartesian trees
  used to implement fast large free heaps. So, for example, the compact large utility free heap relies on the
  utility heap.
- The *large utility free heap* is a fast large free heap that supports decommitting free memory and allocates
  its memory from the bootstrap heap.

Note how the heaps pull memory from one another. Generally, a heap will not return memory to the heap it got
memory from except to "undo" part of an allocation it had just done. So, this arrangement of
who-pulls-memory-from-who is designed for type safety, memory efficiency, and elegantly supporting weird
alignments:

- Libpas uses decommit rather than unmapping free memory because this ensures that we don't ever change the type
  of memory after that memory gets its type for the first time.
- The lower-level heaps (like bootstrap and compact bootstrap) do not support decommit. So, if a higher-level
  heap that does support decommit ever returned memory to the lower-level heap, then the memory would never get
  decommitted.
- Page allocation APIs don't let us easily allocate with alignment greater than page size. Libpas does this by
  over-allocating (allocating size + alignment and then searching for the first aligned start within that larger
  reservation). This is all hidden inside the bootstrap heap; all other heaps that want memory on some weird
  alignment just ask some other heap for memory (often the bootstrap heap) and that heap, or ultimately the
  bootstrap heap, figure out what that means in terms of system calls.

One missing piece to the metacircularity is having a *fast utility heap* that uses thread-local caches. There is
currently maybe one utility heap callsite that only grabs the heap lock just because it wants to allocate in the
utility heap. There's a possibility of a small speed-up if any callsite like that used a fast utility heap
instead, and then no locking would be required. It's not clear how easy that would be; it's possible that some
bad hacks would be required to allow code that uses TLCs to call into a heap that then also uses TLCs.

## Segregated Heaps and TLCs (Thread Local Caches)

Libpas's great performance is mostly due to the segregated heaps and how they leverage thread-local caches. TLCs
provide a cache of global memory which. In the best case, this cache prevents threads from doing any
synchronization during allocation and deallocation. Even when they do have to do some synchronization, TLCs
make it unlikely that one thread will ever want to acquire a lock held by another thread. The strategy is
three-fold:

- The TLC has a per-size-class allocator that caches some amount of that size class's memory. This means that
  the allocation fast path doesn't have to do any locking or atomic instructions except when its cache runs out.
  Then, it will have to do some synchronization -- in libpas's case, fine-grained locking and some lock-free
  algorithms -- to get more memory. The amount of memory each allocator can cache is bounded (usually 16KB) and
  allocators can only hold onto memory for about a second without using it before it gets returned (see the
  scavenger section).
- The TLC has a deallocation log. The fast path of deallocating a segregated heap object is just pushing it onto
  the deallocation log without any locking. The slow path is to walk the log and free all of the objects. The
  libpas deallocation log flush algorithm cleverly avoids doing per-object locking; in the best case it will
  acquire a couple of locks before flushing and release them after flushing the whole log.
- When the deallocation log flush frees memory, it tries to first make that memory available exclusively to the
  thread that freed it by putting the free memory into the *local view cache* for that size class in that
  thread. Memory moves from the local view caches into the global heap only if the view cache is full (has about
  1.6MB of memory in it) or if it hasn't been used in about a second (see the scavenger section).

This section lays out the details of how this works. *Segregated heaps* are organized into *segregated
directories*. Each segregated directory is an array of page *views*. Each page view may or may not have a *page*
associated with it. A view can be *exclusive* (the view has the page to itself), *partial* (it's a view into a
page shared by others), or *shared* (it represents a page shared by many partial views). Pages have a *page
boundary* (the address of the beginning of the page) and a *page header* (the object describing the page, which
may or may not actually be inside the page). Pages maintain *alloc bits* to tell which objects are live.
Allocation uses the heap's lookup tables to find the right *allocator index* in the TLC, which yields a *local
allocator*; that allocator usually has a cache of memory to allocate from. When it doesn't, it first tries to
pop a view from the local view cache, and if that fails, it uses the *find first eligible* algorithm on the
corresponding directory to find an eligible view. One the allocator has a view, it ensures that the view has a
page, and then scans the alloc bits to create a cache of free memory in that page. Deallocation fast paths just
push the object onto the deallocation log. When the log is full, the TLC flushes its log while trying to
amortize lock acquisition. Freeing an object in a page means clearing the corresponding alloc bit. Once enough
alloc bits are clear, either the page's view ends up on the view cache, or the directory is notified to mark the
page either eligible or empty. The sections that follow go into each of these concepts in detail.

### The Thread Local Cache

Each thread has zero or one `pas_thread_local_cache`s. Libpas provides slow paths for allocating and
deallocating without a TLC in cases where TLC creation is forbidden (like when a thread is shutting down). But
usually, allocation and deallocation create a TLC if the thread doesn't already have one. TLCs are structured
as follows:

- TLCs contain a fixed-size deallocation log along with an index that tells how much of the log is full.
  Deallocation pushes onto that log.
- TLCs contain a variable-length array of *allocators*, which are really either *local allocators* or *local
  view caches*. Allocators are variable length. Clients access allocators using an allocator index, which they
  usually get from the directory that the allocator corresponds to.
- TLCs can get reallocated and deallocated, but they always point to a `pas_thread_local_cache_node`, which is
  an immortal and compact descriptor of a TLC. TLC nodes are part of a global linked list. Each TLC node may or
  may not have a live TLC associated with it. TLCs cannot be created or destroyed unless the heap lock is held,
  so if you hold the heap lock, you can iterate the TLC node linked list to find all TLCs.
- The layout of allocator indices in a TLC is controlled by both the directories and the TLC layout data
  structure (`pas_thread_local_cache_layout`). This is a global data structure that can tell us about all of the
  allocators in a TLC. When holding the heap lock, it's possible to loop over the TLC layout linked list to
  find what all of the valid allocator indices are and to introspect what is at those indices.

Thread local caches tend to get large because both the local allocators and local view caches have inline
arrays. The local allocator has an array of bits that tell the allocator where the free objects are. The local
view cache has an array of view pointers (up to 1.6MB / 16KB = 100 entries, each using a 24-bit pointer). When
used in single-heap applications, these overheads don't matter -- they end up being accounting for less than
10<sup>-5</sup> of the overall process footprint (not just in WebKit but when I experimented with libpas in
daemons). But when used for many heaps, these overheads are substantial. Given thousands or tens of thousands
of heaps, TLCs account for as much as 1% of memory. So, TLCs support partial decommit. Those pages that only
have allocators that are inactive get decommitted. Note that TLC decommit has landed in the libpas.git repo
as of a361efa96ca4b2ff6bdfc28bc7eb1a678cde75be, but hasn't yet been merged into WebKit.

The TLC deallocation log flush algorithm is designed to achieve two performance optimizations:

- It achieves temporal locality of accesses to page headers. If freeing each object meant flipping a bit in the
  page header, then many of those operations would miss cache, since the page header is not accessed by normal
  program operation -- it's only accessed during some allocation slow paths and when deallocating. But because
  deallocation accesses page headers only during log flush and log flush touches about 1000 objects, it's likely
  that the flush will touch the same page header cache lines multiple times.
- It reduces the average number of lock acquisitions needed to free an object. Each page uses its own lock to
  protect its page header, and the page header's `alloc_bits`. But deallocation log flush will do no locking or
  unlocking if the object at index *i* and the object at index *i+1* use the same lock. Pages can dynamically
  select which lock they use (thanks to `page->lock_ptr`), and they select it so that pages allocated out of by
  a thread tend to share the same lock, so deallocation log flush usually only just acquires a lock at the start
  of the 1000 objects and releases it when it finishes the 1000 objects.

### The Segregated Heap

The `pas_segregated_heap` object is the part of `pas_heap` that facilitates segregated heap and bitfit heap
allocation. The details of the bitfit heap are discussed in a later section. Segregated heaps can be created
separately from a `pas_heap`, but segregated heaps are almost always part of a `pas_heap` (and it would be easy
to refactor libpas to make it so that segregated heaps are always part of heaps).

Segregated heaps use *size directories* to track actual memory. Most of the exciting action of allocation and
deallocation happens in directories. Each directory corresponds to some size class. Segregated heaps make it
easy to find the directory for a particular size. They also make it easy to iterate over all the directories.
Also, segregated heaps make it easy to find the allocator index for a particular size (using a lookup table that
is essentially a cache of what you would get if you asked for the directory using the size-to-directory lookup
tables and then asked the directory for the allocator index). The most exciting part of the segregated heap
algorithm is `pas_segregated_heap_ensure_size_directory_for_size`, which decides what to do about allocating a
size it hadn't encountered before. This algorithm will either return an existing directory, create a new one, or
even retire an existing one. It handles all of the issues related to type size, type alignment, and the
alignment argument to the current malloc call.

The lookup tables maintained by segregated heaps have some interesting properties:

- They can be decommitted and rematerialized. This is a useful space saving when having lots of isoheaps. The
  rematerialization happens because a heap also maintains a linked list of directories, and that linked list
  never goes away. Each directory in the linked list knows what its representation would have been in the lookup
  tables.
- They are optional. Some heaps can be configured to have a preferred size, called the *basic size class*. This
  is very common for isoheaps, which may only ever allocate a single size. For isoheaps based on type, the
  basic size class is just that type's size. Other isoheaps dynamically infer a preferred size based on the
  first allocation. When a heap only has the basic size class, it will have no lookup tables.
- There are separate lookup tables for smaller sizes (not related to the small_segregated_config -- the
  threshold is set separately) are just arrays whose index is the size divided by the heap's minalign, rounded
  up. These may be populated under heap lock while they are accessed without any locks. So, accesses to them
  have some guards against races.
- There are separate lookup tables for medium sizes (anything above the threshold for small lookup tables).
  The medium table is a sorted array that the allocator binary-searches. It may be mutated, decommitted,
  rematerialized, or reallocated under heap lock. The algorithm defends itself against this with a bunch of
  compiler fences, a mutation count check. Mutating the table means incrementing-to-mutate before making changes
  and incrementing-to-finish after making changes. So, the algorithm for lock-free lookup checks mutation count
  before and after, and makes sure they are the same and neither indicates that we are mutating. This involves
  clever use of dependency threading (like the ARM64 eor-self trick) to make sure that the mutation count reads
  really happen before and after the binary search.

### Segregated Directories

Much of the action of managing memory in a segregated heap happens in the segregated directories. There are two
kinds:

- Segregated size directories, which track the views belonging to some size class in some heap. These may be
  *exclusive views*, which own a page, or *partial views*, which own part of a *shared page*. Partial views
  range in size between just below 512 bytes to possibly a whole page (in rare cases).
- Segregated shared page directories, which track *shared views*. Each shared view tracks shared page and which
  partial views belong to it. However, when a shared page is decommitted, to save space, the shared view will
  forget which partial views belong to it; they will re-register themselves the first time someone allocates in
  them.

Both of them rely on the same basic state, though they use it a bit differently:

- A lock-free-access vector of compact view pointers. These are 4-byte pointers. This is possible because views
  are always allocated out of the compact reservation (they are usually allocated in the immortal
  heap). This vector may be appended to, but existing entries are immutable. So, resizing just avoids deleting
  the smaller-sized vectors so that they may still be accessed in case of a race.
- A lock-free-access segmented vector of bitvectors. There are two bitvectors, and we interleave their 32-bit
  words of bits. The *eligible* bitvector tells us which views may be allocated out of. This means different
  things for size directories than shared page directories. For size directories, these are the views that have
  some free memory and nobody is currently doing anything with them. For shared page directories, these are the
  shared pages that haven't yet been fully claimed by partial views. The *empty* bitvector tells us which pages
  are fully empty and can be decommitted. It's never set for partial views. It means the same thing for both
  exclusive views in size directories and shared views in shared page directories.

Both bitvectors are searched in order:

- Eligible bitvectors are searched first-fit.
- Empty bitvectors are searched last-fit.

Searches are made fast because the directory uses the lock-free tricks of `pas_versioned_field` to maintain two
indices:

- A first-eligible index. This always points to the first eligible bit, except in cases where some thread has
  set the bit but hasn't gotten around to setting the first-eligible index. In other words, this may have some
  lag, but the lag is bounded.
- A last-empty-plus-one index. This always points to the index right after the last empty bit. If it's zero, it
  means there are no set empty bits. If it's the number of views, then it means the last view is empty for sure
  and there may be any number of other empty views.

These versioned indices can be read without any atomic instructions in many cases, though most mutations to them
require a pair of 128-bit compare-and-swaps.

The eligible bitvector together with the first-eligible index allow for very fast searches to find the first
eligible view. Bitvector searches are fast to begin with, even over a segmented vector, since the segmented
vector has large-enough chunks. Even searching the whole bitvector is quite efficient because of the properties
of bitvector simd (i.e. using a 32-bit or 64-bit or whatever-bit word to hold that many bits). But the
first-eligible index means that most searches never go past where that index points, so we get a mostly-O(1)
behavior when we do have to find the first eligible view.

The empty bitvector gives a similar property for the scavenger, which searches backwards to find empty views.
The efficiency here arises from the fact that empty pages know the timestamp of when they became empty, and the
scavenger will terminate its backwards search when it finds a too-recently emptied page.

Directories have to make some choices about how to add views. View addition happens under heap lock and must be
in this order:

- First we make sure that the bitvectors have enough room for a bit at the new index. The algorithm relies on
  the size of the view vector telling us how many views there are, so it's fine for the bitvectors are too big
  for a moment. The segmented vector algorithm used for the bitvectors requires appending to happen under heap
  lock but it can run concurrently to accesses to the vector. It accomplishes this by never deallocating the
  too-small vector spines.
- Then we append the the view to the view vector, possibly reallocating the view vector. Reallocation keeps the
  old two-small copy of the vector around, allowing concurrent reads to the vector. The vector append stores the
  value, executes an acqrel fence (probably overkill -- probably could just be a store fence), and then
  increments the size. This ensures nobody sees the view until we are ready.

Directories just choose what kind of view they will create and then create an empty form of that view. So, right
at the point where the vector append happens, the view will report itself as not yet being initialized. However,
any thread can initialize an empty view. The normal flow of allocation means asking a view to "start
allocating". This actually happens in two steps (`will_start_allocating` and `did_start_allocating`). The
will-start step checks if the view needs to commit its memory, which will cause empty exclusive views to
allocate a page. Empty partial views get put into the *partial primordial* state where they grab their first
chunk of memory from some shared view and prepare to possibly grab more chunks of that shared view, depending on
demand. But all of this happens after the directory has created the view and appended it. This means that there
is even the possibility that one thread creates a view, but then some other thread takes it right after it was
appended. In that case, the first thread will loop around and try again, maybe finding some other view that had
been made eligible in the meantime, or against appending another new view.

Size directories maintain additional state to make page management easy and to accelerate allocation.

Size directories that have enabled exclusive views have a `full_alloc_bits` vector that has bits set for those
indices in their pages where an object might start. Pages use bitvectors indexed by minalign, and only set those
bits that correspond to valid object offsets. The `full_alloc_bits` vector is the main way that directories tell
where objects could possibly be in the page. The other way they tell is with
`offset_from_page_boundary_to_first_object` and `offset_from_page_boundary_to_end_of_last_object`, but the
algorithm relies on those a bit less.

Size directories can tell if they have been assigned an allocator index or a view cache index, and control the
policies of when they get them. A directory without an allocator index will allocate out of *baseline
allocators*, which are shared by all threads. Having an allocator index implies that the allocator index has
also been stored in the right places in the heap's lookup tables. Having a view cache index means that
deallocation will put eligible pages on the view cache before marking them eligible in the directory.

### Page Boundaries, Headers and Alloc Bits

"Pages" in the segregated heap are a configurable concept. Things like their size and where their header lives
can be configured by the `pas_segregated_page_config` of their directory. The config can tell which parts of
the page are usable as object payload, and they can provide callbacks for finding the page header.

The page header contains:

- The page's kind. The segregated page header is a subtype of the `pas_page_base`, and we support safely
  downcasting from `pas_page_base*` to `pas_segregated_page*`. Having the kind helps with this.
- Whether the page is in use for allocation right now. This field is only for exclusive views. Allocation in
  exclusive pages can only happen when some local allocator claims a page. For shared pages, this bit is in
  each of the the partial views.
- Whether we have freed an object in the page while also allocating in it. This field is only for exclusive
  views. When we finish allocating, and the bit is set, we do the eligibility stuff we would have done if we had
  freed objects without the page being used for allocation. For shared pages, this bit is in each of the partial
  views.
- The sizes of objects that the page manages. Again, this is only used for exclusive views. For shared pages,
  each partial view may have a different size directory, and the size directory tells the object size. It's also
  possible to get the object size by asking the exclusive view for its size directory, and you will get the same
  answer as if you had asked the page in that case.
- A pointer to the lock that the page uses. Pages are locked using a kind of locking dance: you load the
  `page->lock_ptr`, lock that lock, and then check if `page->lock_ptr` still points at the lock you tried.
  Anyone who holds both the current lock and some other lock can change `page->lock_ptr` to the other lock.
  For shared pages, the `lock_ptr` always points at the shared view's ownership lock. For exclusive views, the
  libpas allocator will change the lock of a page to be the lock associated with their TLC. If contention
  on `page->lock_ptr` happens, then we change the lock back to the view's ownership lock. This means that in
  the common case, flushing the deallocation log will encounter page after page that wants to hold the same
  lock -- usually the TLC lock. This allows the deallocation log flush to only do a handful of lock acquisitions
  for deallocating thousands of objects.
- The timestamp of when the page became empty, using a unit of time libpas calls *epoch*.
- The view that owns the page. This is either an exclusive view or a *shared handle*, which is the part of the
  shared view that gets deallocated for decommitted pages. Note: an obvious improvement is if shared handles
  were actually part of the page header; they aren't only because until recently, the page header size had to be
  the same for exclusive and shared pages.
- View cache index, if the directory enabled view caching. This allows deallocation to quickly find out which
  view cache to use.
- The alloc bits.
- The number of 32-bit alloc bit words that are not empty.
- Optionally, the granule use counts. It's possible for the page config to say that the page size is larger than
  the system page size, but that the page is divided up into *granules* which are system page size. In that
  case, the page header will have an array of 1-byte use counts per granule, which count the number of objects
  in that granule. They also track a special state when the granule is decommitted. The medium_segregated_config
  uses this to offer fine-grained decommit of 128KB "pages".

Currently we have two ways of placing the page header: either at the beginning of the page, or what we call
the page boundary, or in an object allocated in the utility heap. In the latter case, we use the
mostly-lock-free page header table to map between the page boundary and page header, or vice-versa. The page
config has callbacks that allow either approach. I've also used page config hacking to attempt other kinds of
strategies, like saying that every aligned 16MB chunk of pages has an array of page headers at the start of it;
but those weren't any better than either of the two current approaches.

The most important part of the page header is the alloc bits array and the `num_non_empty_words` counter. This
is where most of the action of allocating and deallocating happens. The magic of the algorithm arises from the
simple bitvector operations we can perform on `page->alloc_bits`, `full_alloc_bits` (from the size
directory in case of exclusive pages or from the partial view in case of shared pages), and the
`allocator->bits`. These operations allow us to achieve most of the algorithm:

- Deallocation clears a bit in `page->alloc_bits` and if this results in the word becoming zero, it decrements
  the `num_non_empty_words`. The bit index is just the object's offset shifted by the page config's
  `min_aligh_shift`, which is a compile-time constant in most of the algorithm. If the algorithm makes any bit
  (for partials) or any word (for exclusives) empty, it makes the page eligible (either by putting it on a view
  cache or marking the view eligible in its owning directory). If `num_non_empty_words` hits zero, the
  deallocator also makes the view empty.
- Allocation does a find-first-set-bit on the `allocator->bits`, but in a very efficient way, because the
  current 64-bit word of bits that the allocator is on is cached in `allocator->current_word` -- so allocation
  rarely searches an array. So, the allocator just loads the current word, does a `ctz` or `clz` kind of
  operation (which is super cheap on modern CPUs), left-shifts the result by the page config's minalign, and
  adds the `allocator->page_ish` (the address in memory corresponding to the first bit in `current_word`).
  That's the allocator fast path.
- We prepare `allocator->bits` by basically saying `allocator->bits = full_alloc_bits & ~page->alloc_bits`. This
  is a loop since each of the `bits` is an array of words of bits and each array is the same size. For a 16384
  size page (the default for `small_segregated_config`) and a minalign shift of 4 (so minalign = 16, the default
  for `small_segregated_config`), this means 1024 bits, or 32 32-bit words, or 128 bytes. The loop over the 32
  32-bit words is usually fully unrolled by the compiler. There are no loop-carried dependencies. This loop
  shows up in profiles, and though I've tried to make it faster, I've never succeeded.

### Local Allocators

Each size directory can choose to use either baseline allocators or TLC local allocators for allocation. Each
size directory can choose to have a local view cache or not. Baseline allocators are just local allocators that
are global and not part of any TLC and allocation needs to grab a lock to use them. TLC local allocators don't
require any locking to get accessed.

Local allocators can be in any of these modes:

- They are totally uninitialized. All fast paths fail and slow paths will initialize the local allocator by
  asking the TLC layout. This state happens if TLC decommit causes a local allocator to become all zero.
- They are in bump allocation mode. Bump allocation happens either when a local allocator decides to allocate in
  a totally empty exclusive page, or for primordial partial allocation. In the former case, it's worth about 1%
  performance to sometimes bump-allocate. In the latter case, using bump allocation is just convenient -- the
  slow path will decide that the partial view should get a certain range of memory within a shared page and it
  knows that this memory has never been used before, so it's natural to just set up a bump range over that
  memory.
- They are in free bits mode. This is slightly more common than the bump mode. In this mode, the
  `allocator->bits` is computed using `full_alloc_bits & ~page->alloc_bits` and contains a bit for the start of
  every free object.
- They are in bitfit mode. In this mode, the allocator just forwards allocations to the `pas_bitfit_allocator`.

Local allocators can be *stopped* at any time; this causes them to just return all of their free memory back to
the heap.

Local allocators in a TLC can be used without any conventional locking. However, there is still synchronization
taking place because the scavenger is allowed to stop allocators. To support this, local allocators set an
`in_use` bit (not atomically, but protected by a `pas_compiler_fence`) before they do any work and clear it when
done. The scavenger thread will suspend threads that have TLCs and then while the TLC is suspended, they can
stop any allocators that are not `in_use`.

## The Bitfit Heaps

Libpas is usually used with a combination of segregated and large heaps. However, sometimes we want to have a
heap that is more space-efficient than segregated but not quite as slow as large. The bitfit heap follows a
similar style to segregated, but:

- While bitfit has a bit for each minalign index like segregated, bitfit actually uses all of the bits. To
  allocate an object in bitfit, all of the bits corresponding to all of the minalign granules that the object
  would use have to be free before the allocation and have to be marked as not free after the allocation.
  Freeing has to clear all of the bits.
- The same page can have objects of any size allocated in it. For example, if a 100 byte object gets freed, then
  it's legal to allocate two 50 byte objects out of the freed space (assuming 50 is a multiple of minalign).
- A bitfit directory does not represent a size class. A bitfit heap has one directory per bitfit page config
  and each page config supports a large range of sizes (the largest object is ~250 times larger than the
  smallest).

Bitfit pages have `free_bits` as well as `object_end_bits`. The `free_bits` indicates every minalign granule
that is free. For non-free granules, the `object_end_bits` has an entry for every granule that is the last
granule in some live object. These bits get used as follows:

- To allocate, we find the first set free bit and then find the first clear free bit after that. If this range
  is big enough for the allocation, we clear all of the free bits and set the object end bit. If it's not big
  enough, we keep searching. We do special things (see below) when we cannot allocate in a page.
- To free, we find the first set object end bit, which then gives us the object size. Then we clear the object
  end bit and set the free bits.

This basic style of allocation is usually called *bitmap* allocation. Bitfit is a special kind of bitmap
allocation that makes it cheap to find the first page that has enough space for an allocation of a given size.
Bitfit makes allocation fast even when it is managing lots of pages by using two tricks:

- Bitfit directories have an array of bitfit views and a corresponding array of `max_free` bytes. Bitfit views
  are monomorphic, unlike the polymorphic views of segregated heaps. Each bitfit view is either uninitialized or
  has a bitfit page. A `max_free` byte for a page tells the maximum free object size in that page. So, in the
  worst case, we search the `max_free` vector to find the find byte that is large enough for our allocation.
- Bitfit uses size classes to short-circuit the search. Bitfit leverages segregated heaps to create size
  classes. Segregated size directories choose at creation time if they want to support segregated allocation or
  bitfit allocation. If the latter, the directory is just used as a way of locating the bitfit size class. Like
  with segregated, each local allocator is associated with a segregated size directory, even if it's a local
  allocator configured for bitfit. Each size class maintains the index of the first view/page in the directory
  that has a free object big enough for that size class.

The updates to `max_free` and the short-circuiting indices in size classes happen when an allocation fails in
a page. This is an ideal time to set those indices since failure to allocate happens to also tell you the size
of the largest free object in the page.

When any object is freed in a page, we mark the page as having `PAS_BITFIT_MAX_FREE_UNPROCESSED` and rather than
setting any short-circuiting indices in size classes, we just set the `first_unprocessed_free` index in the
`pas_bitfit_directory`. Allocation will start its search from the minimum of `directory->first_unprocessed_free`
and `size_class->first_free`. All of these short-circuiting indices use `pas_versioned_field` just like how
short-circuiting works in segregated directories.

Bitfit heaps use fine-grained locking in the sense that each view has its own locks. But, there's no attempt
made to have different threads avoid allocating in the same pages. Adding something like view caches to the
bitfit heap is likely to make it much faster. You could even imagine that rather than having the
`directory->first_unprocessed_free`, we instead have freeing in a page put the page's view onto a local view
cache for that bitfit directory, and then we allocate out of the view cache until it's empty. Failure to
allocate in a page in the view cache will then tell us the `max_free`, which will allow us to mark the view
eligible in the directory.

## The Scavenger

Libpas returns memory to the OS by madvising it. This makes sense for a malloc that is trying to give strong
type guarantees. If we unmapped memory, then the memory could be used for some totally unrelated type in the
future. But by just decommitting the memory, we get the memory savings from free pages of memory and we also
get to preserve type safety.

The madvise system call -- and likely any mechanism for saying "this page is empty" -- is expensive enough that
it doesn't make sense to do it anytime a page becomes empty. Pages often become empty only to refill again. In
fact, last time I measured it, just about half of allocations went down the bump allocation path and (except for
at start-up) that path is for completely empty pages. So, libpas has mechanisms for stashing the information
that a page has become empty and then having a *scavenger thread* return that memory to the OS with an madvise
call (or whatever mechanism). The scavenger thread is by default configured to run every 100ms, but will shut
itself down if we have a period of non-use. At each tick, it returns all empty pages that have been empty for
some length of time (currently 300ms). Those two thresholds -- the period and the target age for decommit -- are
independently configurable at it might make sense (for different reasons) to have either number be bigger than
the other number.

This section describes the scavenging algorithm is detail. This is a large fraction of what makes libpas fast
and space-efficient. The algorithm has some crazy things in it that probably didn't work out as well as I wanted
but nonetheless those things seem to avoid showing up in profiles. That's sort of the outcome of this algorithm
being tuned and twisted so many times during the development of this allocator. First I'll describe the
*deferred decommit log*, which is how we coalesce madvise calls. Then I'll describe the *page sharing pool*,
which is a mechanism for multiple *participants* to report that they have some empty pages. Then I'll describe
how the large heap implements this with the large sharing pool, which is one of the singleton participants. Then
I'll describe the segregated directory participants -- which are a bit different for shared page directories
versus size directories. I'll also describe the bitfit directory participants, which are quite close to their
segregated directory cousins. Then I'll describe some of the things that the scavenger does that aren't to do
with the page sharing pool, like stopping baseline allocators, stopping utility heap allocators, stopping
TLC allocators, flushing TLC deallocation logs, decommitting unused parts of TLCs, and decommitting expendable
memory.

### Deferred Decommit Log

Libpas's decommit algorithm coalesces madvise calls -- so if two adjacent pages, even from totally unrelated
heaps, become empty, then their decommit will be part of one syscall. This is achieved by having places in the
code that want to decommit memory instead add that memory to a deferred decommit log. This log internally uses
a minheap based on address. The log also stores what lock was needed to decommit the range, since the decommit
algorithm relies on fine-grained locking of memory rather than having a global commit lock. So, when the
scavenger asks the page sharing pool to go find some memory to decommit, it gives it a deferred decommit log.
The page sharing pool will usually return all of the empty pages in one go, so the deferred decommit logs can
get somewhat big. They are allocated out of the bootstrap free heap (which isn't the best idea if they ever get
very big, since bootstrap free heap memory is not decommitted -- but right now this is convenient because for
a heap to support decommit, it needs to talk to deferred decommit logs, so we want to avoid infinite recursion).
After the log is filled up, we can decommit everything in the log at once. This involves heapifying the array
and then scanning it backwards while detecting adjacent ranges. This is the loop that actually calls decommit. A
second loop unlocks the locks.

Two fun complications arise:

- As the page sharing pool scans memory for empty pages, it may arrive at pages in a random order, which may
  be different from any valid order in which to acquire commit locks. So, after acquiring the first commit lock,
  all subsequent lock acquisitions are `try_lock`'s. If any `try_lock` fails, the algorithm returns early, and
  the deferred decommit log helps facilitate detecting when a lock failed to be acquired.
- Libpas supports calling into the algorithm even if some commit locks, or the heap lock, are held. It's not
  legal to try to acquire any commit locks other than by `try_lock` in that case. The deferred decommit log will
  also make sure that it will not relock any commit locks that are already held.

Libpas can be configured to return memory either by madvising it or by mmap-zero-filling it, which has a similar
semantic effect but is slower. Libpas supports both symmetric and asymmetric forms of madvise, though the
asymmetric form is faster and has a slight (though mostly theoretical) memory usage edge. By *asymmetric* I mean
that you call some form of madvise to decommit memory and then do nothing to commit it. This works on Darwin and
it's quite efficient -- the kernel will clear the decommit request and give the page real memory if you access
the page. The memory usage edge of not explicitly committing memory in the memory allocator is that programs may
allocate large arrays and never use the whole array. It's OK to configure libpas to use symmetric decommit, but
the asymmetric variant might be faster or more efficient, if the target OS allows it.

### Page Sharing Pool

Different kinds of heaps have different ways of discovering that they have empty pages. Libpas supports three
different kinds of heaps (large, segregated, and bitfit) and one of those heaps has two different ways of
discovering free mempry (segregated shared page directories and segregated size directories). The page sharing
pool is a data structure that can handle an arbitrary number of *page sharing participants*, each of which is
able to say whether they have empty pages and whether those empty pages are old enough to be worth decommitting.

Page sharing participants need to be able to answer the following queries:

- Getting the epoch of the oldest free page. This can be approximate; for example, it's OK for the participant
  to occasionally give an epoch that is newer than the true oldest so long as this doesn't happen all the time.
  We call this the `use_epoch`.
- Telling if the participant thinks it *is eligible*, i.e. has any free pages right now. It's OK for this to
  return true even if there aren't free pages. It's not OK for this to return false if there are free pages. If
  this returns true and there are no free pages, then it must return false after a bounded number of calls to
  `take_least_recently_used`. Usually if a participant incorrectly says that it is eligible, then this state
  will clear with exactly one call to `take_least_recently_used`.
- Taking the least recently used empty page (`pas_page_sharing_participant_take_least_recently_used`). This is
  allowed to return that there aren't actually any empty pages. If there are free pages, this is allowed to
  return any number of them (doesn't have to be just one). Pages are "returned" via a deferred decommit log.

Participants also notify the page sharing pool when they see a *delta*. Seeing a delta means one of:

- The participant found a new free page and it previously thought that it didn't have any.
- The participant has discovered that the oldest free page is older than previously thought.

It's OK to only report a delta if the participant knows that it was previously not advertising itself as being
eligible, and it's also OK to report a delta every time that a free page is found. Most participants try to
avoid reporting deltas unless they know that they were not previously eligible. However, some participants (the
segregated and bitfit directories) are sloppy about reporting the epoch of the oldest free page. Those
participants will conservatively report a delta anytime they think that their estimate of the oldest page's age
has changed.

The page sharing pool itself comprises:

- A segmented vector of page sharing participant pointers. Each pointer is tagged with the participant's type,
  which helps the pool decide how to get the `use_epoch`, decide whether the participant is eligible, and take
  free pages from it.
- A bitvector of participants that have reported deltas.
- A minheap of participants sorted by epoch of the oldest free memory.
- The `current_participant`, which the page sharing pool will ask for pages before doing anything else, so long
  as there are no deltas and the current participant continues to be eligible and continues to report the same
  use epoch.

If the current participant is not set or does not meet the criteria, the heap lock is taken and all of the
participants that have the delta bit set get reinserted into the minheap based on updated use epochs. It's
possible that the delta bit causes the removal of entries in the minheap (if something stopped being eligible).
It's possible that the delta bit causes the insertion of entries that weren't previously there (if something has
just become eligible). And it's possible for an entry to be removed and then readded (if the use epoch changed).
Then, the minimum of the minheap becomes the current participant, and we can ask it for pages.

The pool itself is a class but it happens to be a singleton right now. It's probably a good idea to keep it as a
class, because as I've experimented with various approaches to organizing memory, I have had versions where
there are many pools. There is always the physical page sharing pool (the singleton), but I once had sharing
pools for moving pages between threads and sharing pools for trading virtual memory.

The page sharing pool exposes APIs for taking memory from the pool. There are two commonly used variants:

- Take a set number of bytes. The page sharing pool will try to take that much memory unless a try_lock fails,
  in which case it records that it should take this additional amount of bytes on the next call.
- Take all free pages that are same age or older than some `max_epoch`. This API is called
  `pas_physical_page_sharing_pool_scavenge`. This algorithm will only return when it is done. It will reloop in
  case of `try_lock` failure, and it does this in a way that avoids spinning.

The expected behavior is that when the page sharing pool has to get a bunch of pages it will usually get a run
of them from some participant -- hence the emphasis on the `current_participant`. However, it's possible that
various tweaks that I've made to the algorithm have made this no longer be the case. In that case, it might be
worthwhile to try to come up with a way of recomputing he `current_participant` that doesn't require holding the
`heap_lock`. Maybe even just a lock for the page sharing pool, rather than using the `heap_lock`, would be
enough to get a speed-up, and in the best case that speed-up would make it easier to increase scavenger
frequency.

The page sharing pool's scavenge API is the main part of what the scavenger does. The next sections describe the
inner workings of the page sharing participants.

### Large Sharing Pool

The large sharing pool is a singleton participant in the physical page sharing pool. It tracks which ranges of
pages are empty across all of the large heaps. The idea is to allow large heaps to sometimes split a page among
each other, so then no single large heap knows whether the page can be decommitted. So, all large heaps as well
as the large utility free heap and compact large utility free heap report when they allocate and deallocate
memory to the large sharing pool.

Internally, the large sharing pool maintains a red-black tree and minheap. The tree tracks coalesced ranges of
pages and their states. The data structure thinks it knows about all of memory, and it "boots up" with a single
node representing the whole address space and that node claims to be allocated and committed. When heaps that
talk to the large sharing pool acquire memory, they tell the sharing pool that the memory is now free. Any
free-and-committed ranges also reside in the minheap, which is ordered by use epoch (the time when the memory
became free).

The large sharing pool can only be used while holding the `heap_lock`, but it uses a separate lock, the
`pas_virtual_range_common_lock`, for commit and decommit. So, while libpas is blocked in the madvise syscall,
it doesn't have to hold the `heap_lock` and the other lock only gets acquired when committing or decommitting
large memory.

It's natural for the large sharing pool to handle the participant API:

- The large sharing pool participant says it's eligible when the minheap is non-empty.
- The large sharing pool participant reports the minheap's minimum use epoch as its use epoch (though it can be
  configured to do something else; that something else may not be interesting anymore).
- The large sharing pool participant takes least recently used by removing the minimum from the minheap and
  adding that node's memory range to the deferred decommit log.

The large sharing pool registers itself with the physical page sharing pool when some large heap reports the
first bit of memory to it.

### Segregated Directory Participant

Each segregated directory is a page sharing participant. They register themselves once they have pages that
could become empty. Segregated directories use the empty bits and the last-empty-plus-one index to satisfy the
page sharing pool participant API:

- A directory participant says it's eligible when last-empty-plus-one is nonzero.
- A directory participant reports the use epoch of the last empty page before where last-empty-plus-one points.
  Note that in extreme cases this means having to search the bitvector for a set empty bit, since the
  last-empty-plus-one could lag in being set to a lower value in case of races.
- A directory participant takes least recently used by searching backwards from last-empty-plus-one and taking
  the last empty page. This action also updates last-empty-plus-one using the `pas_versioned_field` lock-free
  tricks.

Once an empty page is found the basic idea is:

1. Clear the empty bit.
2. Try to take eligibility; i.e. make the page not eligible. We use the eligible bit as a kind of lock
   throughout the segregated heap; for example, pages will not be eligible if they are currently used by some
   local allocator. If this fails, we just return. Note that it's up to anyone who makes a page ineligible to
   then check if it should set the empty bit after they make it eligible again. So, it's fine for the scavenger
   to not set the empty bit again after clearing it and failing to take the eligible bit. This also prevents a
   spin where the scavenger keeps trying to look at this allegedly empty page even though it's not eligible. By
   clearing the empty bit and not setting it again in this case, the scavenger will avoid this page until it
   becomes eligible.
3. Grab the *ownership lock* to say that the page is now decommitted.
4. Grab the commit lock and then put the page on the deferred decommit log.
5. Make the page eligible again.

Sadly, it's a bit more complicated than that:

- Directories don't track pages; they track views. Shared page directories have views of pages whose actual
  eligibility is covered by the eligible bits in the segregated size directories that hold the shared page's
  partial views. So, insofar as taking eligibility is part of the algorithm, shared page directories have to
  take eligibility for each partial view associated with the shared view.
- Shared and exclusive views could both be in a state where they don't even have a page. So, before looking at
  anything about the view, it's necessary to take the ownership lock. In fact, to have a guarantee that nobody
  is messing with the page, we need to grab the ownership lock and take eligibility. The actual algorithm does
  these two things together.
- It's possible for the page to not actually be empty even though the empty bit is set. We don't require that
  the empty bit is cleared when a page becomes nonempty.
- Some of the logic of decommitting requires holding the `page->lock_ptr`, which may be a different lock than
  the ownership lock. So the algorithm actually takes the ownership lock, then the page lock, then the ownership
  lock again, and then the commit lock.
- If a `try_lock` fails, then we set both the eligible and empty bits, since in that case, we really do want
  the page sharing pool to come back to us.

Some page configs support segregated pages that have multiple system pages inside them. In that case, the empty
bit gets set when any system page becomes empty (using granule use counts), and the taking algorithm just
decommits the granules rather than decommitting the whole page.

### Bitfit Directory Participant

Bitfit directories also use an empty bitvector and also support granules. Although bitfit directories are an
independent piece of code, their approach to participating in page sharing pools exactly mirrors what segregated
directories do.

This concludes the discussion about the page sharing pool and its participants. Next, I will cover some of the
other things that the scavenger does.

### Stopping Baseline Allocators

The scavenger also routinely stops baseline allocators. Baseline allocators are easy to stop by the scavenger
because they can be used by anyone who holds their lock. So the scavenger can stop any baseline allocator it
wants. It will only stop those allocators that haven't been used in a while (by checking and resetting a dirty
bit).

### Stopping Utility Heap Allocators

The scavenger also does the same thing for utility allocators. This just requires holding the `heap_lock`.

### Stopping TLC Allocators

Allocators in TLCs also get stopped, but this requires more effort. When the scavenger thread is running, it's
possible for any thread to be using any of its allocators and no locks are held when this happens. So, the
scavenger uses the following algorithm:

- First it tries to ask the thread to stop certain allocators. In each allocation slow path, the allocator
  checks if any of the other allocators in the TLC have been requested to stop by the scavenger, and if so, it
  stops those allocators. That doesn't require special synchronization because the thread that owns the
  allocator is the one stopping it.
- If that doesn't work out, the scavenger suspends the thread and stops all allocators that don't have the
  `in_use` bit set. The `in_use` bit is set whenever a thread does anything to a local allocator, and cleared
  after.

One wrinkle about stopping allocators is that stopped allocators might get decommitted. The way that this is
coordinated is that a stopped allocator is in a special state that means that any allocation attempt will take
a slow path that acquires the TLC's scavenging lock and possibly recommits some pages and then puts the
allocator back into a normal state.

### Flushing TLC Deallocation Logs

The TLC deallocation logs can be flushed by any thread that holds the scavenging lock. So, the scavenger thread
flushes all deallocation logs that haven't been flushed recently.

When a thread flushes its own log, it holds the scavenger lock. However, appending doesn't grab the lock. To
make this work, when the scavenger flushes the log, it:

- Replaces any entry in the log that it deallocated with zero. Actually, the deallocation log flush always does
  this.
- Does not reset the `thread_local_cache->deallocation_log_index`. In fact, it doesn't do anything except read
  the field.

Because of the structure of the deallocation log flush, it's cheap for it to null-check everything it loads from
the deallocation log. So when, a thread goes to flush its log after the scavenger has done it, it will see a
bunch of null entries, and it will skip them. If a thread tries to append to the deallocation log while the
scavenger is flushing it, then this just works, because it ends up storing the new value above what the
scavenger sees. The object is still in the log, and will get deallocated on the next flush.

### Decommitting Unused Parts of TLCs

For any page in the TLC consisting entirely of stopped allocators, the scavenger can decommit those pages. The
zero value triggers a slow path in the local allocator and local view cache that then commits the page and
rematerializes the allocator. This is painstakingly ensured; to keep this property you generally have to audit
the fast paths to see which parts of allocators they access, and make sure that the allocator goes to a slow
path if they are all zero. That slow path then has to check if the allocator is stopped or decommitted, and if
it is either, it grabs the TLC's scavenger lock and recommits and rematerializes.

This feature is particularly valuable because of how big local allocators and local view caches are. When there
are a lot of heaps, this accounts for tens of MBs in some cases. So, being able to decommit unused parts is a
big deal.

### Decommitting Expendable Memory

Segregated heaps maintain lookup tables that map "index", i.e. the object size divided by the heap's minalign,
to allocator indices and directories. There are three tables:

- The index-to-allocator-index table. This only works for small-enough indices.
- The index-to-directory table. This also only works for small-enough indices.
- The medium index-to-directory-tuple binary search array. This works for the not-small-enough indices.

It makes sense to let those tables get large. It might make sense for each isoheap to have a large lookup table.
Currently, they use some smaller threshold. But to make that not use a lot of memory, we need to be able to
decommit memory that is only used by lookup tables of heaps that nobody is using.

So, libpas has this weird thing called `pas_expendable_memory`, which allows us to allocate objects of immortal
memory that automatically get decommitted if we don't "touch" them frequently enough. The scavenger checks the
state of all expendable memory, and decommits those pages that haven't been used in a while. The expendable
memory algorithm is not wired up as a page sharing participant because the scavenger really needs to poke the
whole table maintained by the algorithm every time it does a tick; otherwise the algorithm would not work.
Fortunately, there's never a lot of this kind of memory. So, this isn't a performance problem as far as I know.
Also, it doesn't actually save that much memory right now -- but also, right now isoheaps use smaller-size
tables.

This concludes the discussion of the scavenger. To summarize, the scavenger periodically:

- Stops baseline allocators.
- Stops utility heap allocators.
- Stops TLC allocators.
- Flushes TLC deallocation logs.
- Decommits unused parts of TLCs.
- Decommits expendable memory.
- Asks the physical page sharing pool to scavenge.

While it does these things, it notes whether anything is left in a state where it would be worthwhile to run
again. A steady state might look like that all empty pages have been decommitted, rather than that only old
enough ones were decommitted. If a steady state looks like it's being reached, the scavenger first sleeps for
a while, and then shuts down entirely.

## Megapages and Page Header Tables

Libpas provides non-large heaps two fast and scalable ways of figuring out about an object based on its
address, like during deallocation, or during user queries like `malloc_size`.

1. Megapages and the megapage table. Libpas describes every 16MB of memory using a two-bit enum called
   `pas_fast_megapage_kind`. The zero state indicates that this address does not have a *fast megapage*. The
   other two describe two different kinds of fast megapages, one where you know exactly the type of page it
   is (small exclusive segregated) and one where you have to find out by asking the page header, but at least
   you know that it's small (so usually 16KB).
2. Page header table. This is a lock-free-to-read hashtable that tells you where to find the page header for
   some page in memory. For pages that use page headers, we also use the page header table to find out if the
   memory address is in memory owned by some kind of "page" (either a segregated page or a bitfit page).

Usually, the small segregated and small bitfit configs use megapages. The medium segregated, medium bitfit, and
marge bitfit configs use page header tables. Large heaps don't use either, since they have the large map.

## The Enumerator

Libpas supports the libmalloc enumerator API. It even includes tests for it. The way that this is implemented
revolves around the `pas_enumerator` class. Many of the data structures that are needed by the enumerator have
APIs to support enumeration that take a `pas_enumerator*`. The idea behind how it works is conceptually easy:
just walk the heap -- which is possible to do accurately in libpas -- and report the metadata areas, the areas
that could contain objects, and the live objects. But, we have to do this while walking a heap in a foreign
process, and we get to see that heap through little copies of it that we request a callback to make for us. The
code for enumeration isn't all that pretty but at least it's easy to find most of it by looking for functions
and files that refer to `pas_enumerator`.

But a challenge of enumeration is that it happens when the remote process is stopped at some arbitrary
point. That point has to be an instruction boundary -- so CPU memory model issues aren't a problem. But this
means that the whole algorithm has to ensure that at any instruction boundary, the enumerator will see the right
thing. Logic to help the enumerator still understand the heap at any instruction is spread throughout the
allocation algorithms. Even the trees and hashtables used by the large heap have special hacks to enable them to
be enumerable at any instruction boundary.

The enumerator is maintained so that it's 100% accurate. Any discrepancy -- either in what objects are reported
live or what their sizes are -- gets flagged as an error by `test_pas`. The goal should be to maintain perfect
enumerator accuracy. This makes sense for two reasons:

1. I've yet to see a case where doing so is a performance or memory regression.
2. It makes the enumerator easy to test. I don't know how to test an enumerator that is not accurate by design.
   If it's accurate by design, then any discrepancy between the test's understanding of what is live and what
   the enumerator reports can be flagged as a test failure. If it wasn't accurate by design, then I don't know
   what it would mean for a test to fail or what the tests could even assert.

## The Basic Configuration Template

Libpas's heap configs and page configs allow for a tremendous amount of flexibility. Things like the utility
heap and the `jit_heap` leverage this flexibility to do strange things. However, if you're using libpas to
create a normal malloc, then a lot of the configurability in the heap/page configs is too much. A "normal"
malloc is one that is exposed as a normal API rather than being internal to libpas, and that manages memory that
doesn't have special properties like that it's marked executable and not writeable.

The basic template is provided by `pas_heap_config_utils.h`. To define a new config based on this template, you
need to:

- Add the appropriate heap config and page config kinds to `pas_heap_config_kind.def`,
  `pas_segregated_page_config_kind.def`, and `pas_bitfit_page_config_kind.def`. You also have to do this if you
  add any kind of config, even one that doesn't use the template.
- Create the files `foo_heap_config.h` and `foo_heap_config.c`. These are mostly boilerplate.

The header file usually looks like this:

    #define ISO_MINALIGN_SHIFT ((size_t)4)
    #define ISO_MINALIGN_SIZE ((size_t)1 << ISO_MINALIGN_SHIFT)
    
    #define ISO_HEAP_CONFIG PAS_BASIC_HEAP_CONFIG( \
        iso, \
        .activate = pas_heap_config_utils_null_activate, \
        .get_type_size = pas_simple_type_as_heap_type_get_type_size, \
        .get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
        .dump_type = pas_simple_type_as_heap_type_dump, \
        .check_deallocation = true, \
        .small_segregated_min_align_shift = ISO_MINALIGN_SHIFT, \
        .small_segregated_sharing_shift = PAS_SMALL_SHARING_SHIFT, \
        .small_segregated_page_size = PAS_SMALL_PAGE_DEFAULT_SIZE, \
        .small_segregated_wasteage_handicap = PAS_SMALL_PAGE_HANDICAP, \
        .small_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
        .small_shared_segregated_logging_mode = pas_segregated_deallocation_no_logging_mode, \
        .small_exclusive_segregated_enable_empty_word_eligibility_optimization = false, \
        .small_shared_segregated_enable_empty_word_eligibility_optimization = false, \
        .small_segregated_use_reversed_current_word = PAS_ARM64, \
        .enable_view_cache = false, \
        .use_small_bitfit = true, \
        .small_bitfit_min_align_shift = ISO_MINALIGN_SHIFT, \
        .small_bitfit_page_size = PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE, \
        .medium_page_size = PAS_MEDIUM_PAGE_DEFAULT_SIZE, \
        .granule_size = PAS_GRANULE_DEFAULT_SIZE, \
        .use_medium_segregated = true, \
        .medium_segregated_min_align_shift = PAS_MIN_MEDIUM_ALIGN_SHIFT, \
        .medium_segregated_sharing_shift = PAS_MEDIUM_SHARING_SHIFT, \
        .medium_segregated_wasteage_handicap = PAS_MEDIUM_PAGE_HANDICAP, \
        .medium_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_aware_logging_mode, \
        .medium_shared_segregated_logging_mode = pas_segregated_deallocation_no_logging_mode, \
        .use_medium_bitfit = true, \
        .medium_bitfit_min_align_shift = PAS_MIN_MEDIUM_ALIGN_SHIFT, \
        .use_marge_bitfit = true, \
        .marge_bitfit_min_align_shift = PAS_MIN_MARGE_ALIGN_SHIFT, \
        .marge_bitfit_page_size = PAS_MARGE_PAGE_DEFAULT_SIZE)
    
    PAS_API extern pas_heap_config iso_heap_config;
    
    PAS_BASIC_HEAP_CONFIG_DECLARATIONS(iso, ISO);

Note the use of `PAS_BASIC_HEAP_CONFIG`, which creates a config literal that automatically fills in a bunch of
heap config, segregated page config, and bitfit page config fields based on the arguments you pass to
`PAS_BASIC_HEAP_CONFIG`. The corresponding `.c` file looks like this:

    pas_heap_config iso_heap_config = ISO_HEAP_CONFIG;
    
    PAS_BASIC_HEAP_CONFIG_DEFINITIONS(
        iso, ISO,
        .allocate_page_should_zero = false,
        .intrinsic_view_cache_capacity = pas_heap_runtime_config_zero_view_cache_capacity);

Note that this just configures whether new pages are zeroed and what the view cache capacity for the intrinsic
heap are. The *intrinsic heap* is one of the four categories of heaps that the basic heap configuration template
supports:

- Intrinsic heaps are global singleton heaps, like the common heap for primitives. WebKit's fastMalloc bottoms
  out in an intrinsic heap.
- Primitive heaps are heaps for primitive untyped values, but that aren't singletons. You can have many
  primitive heaps.
- Typed heaps have a type, and the type has a fixed size and alignment. Typed heaps allow allocating single
  instances of objects of that type or arrays of that type.
- Flex heaps are for objects with flexible array members. They pretend as if their type has size and alignment
  equal to 1, but in practice they are used for objects that have some base size plus a variable-length array.
  Note that libpas doesn't correctly manage flex memory in the large heap; we need a variant of the large heap
  that knows that you cannot reuse flex memory between different sizes.

The basic heap config template sets up some basic defaults for how heaps work:

- It makes small segregated and small bitfit page configs put the page header at the beginning of the page and
  it arranges to have those pages allocated out of megapages.
- It makes medium segregated, medium bitfit, and marge bitfit use page header tables.
- It sets up a way to find things like the page header tables from the enumerator.
- It sets up segregated shared page directories for each of the segregated page configs.

The `bmalloc_heap_config` is an example of a configuration that uses the basic template. If we ever wanted to
put libpas into some other malloc library, we'd probably create a heap config for that library, and we would
probably base it on the basic heap config template (though we don't absolutely have to).

## JIT Heap Config

The JIT heap config is for replacing the MetaAllocator as a way of doing executable memory allocation in WebKit.
It needs to satisfy two requirements of executable memory allocation:

- The allocator cannot read or write the memory it manages, since that memory may have weird permissions at any
  time.
- Clients of the executable allocator must be able to in-place shrink allocations.

The large heap trivially supports both requirements. The bitfit heap trivially supports the second requirement,
and can be made to support the first requirement if we use page header tables for all kinds of memory, not just
medium or marge. So, the JIT heap config focuses on just using bitfit and large and it forces bitfit to use
page header tables even for the small bitfit page config.

## The Fast Paths

All of the discussion in the previous sections is about the innards of libpas. But ultimately, clients want to
just call malloc-like and free-like functions to manage memory. Libpas provides fast path templates that actual
heap implementations reuse to provide malloc/free functions. The fast paths are:

- `pas_try_allocate.h`, which is the single object allocation fast path for isoheaps. This function just takes a
  heap and no size; it allocates one object of the size and alignment that the heap's type wants.
- `pas_try_allocate_array.h`, which is the array and aligned allocation fast path for isoheaps. You want to use
  it with heaps that have a type, and that type has a size and alignment, and you want to allocate arrays of
  that type or instances of that type with special alignment.
- `pas_try_allocate_primitive.h`, which is the primitive object allocation fast path for heaps that don't have
  a type (i.e. they have the primitive type as their type -- the type says it has size and alignment equal to
  1).
- `pas_try_allocate_intrinsic.h`, which is the intrinsic heap allocation fast path.
- `pas_try_reallocate.h`, which provides variants of all of the allocators that reallocate memory.
- `pas_deallocate.h`, which provides the fast path for `free`.
- `pas_get_allocation_size.h`, which is the fast path for `malloc_size`.

One thing to remember when dealing with the fast paths is that they are engineered so that malloc/free functions
do not have a stack frame, no callee saves, and don't need to save the LR/FP to the stack. To facilitate this,
we have the fast path call an inline-only fast path, and if that fails, we call a "casual case". The inline-only
fast path makes no out-of-line function calls, since if it did, we'd need a stack frame. The only slow call (to
the casual case) is a tail call. For example:

    static PAS_ALWAYS_INLINE void* bmalloc_try_allocate_inline(size_t size)
    {
        pas_allocation_result result;
        result = bmalloc_try_allocate_impl_inline_only(size, 1);
        if (PAS_LIKELY(result.did_succeed))
            return (void*)result.begin;
        return bmalloc_try_allocate_casual(size);
    }

The way that the `bmalloc_try_allocate_impl_inline_only` and `bmalloc_try_allocate_casual` functions are created
is with:

    PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
        bmalloc_try_allocate_impl,
        BMALLOC_HEAP_CONFIG,
        &bmalloc_intrinsic_runtime_config.base,
        &bmalloc_allocator_counts,
        pas_allocation_result_identity,
        &bmalloc_common_primitive_heap,
        &bmalloc_common_primitive_heap_support,
        pas_intrinsic_heap_is_designated);

All allocation fast paths require this kind of macro that creates a bunch of functions -- both the inline paths
and the out-of-line paths. The deallocation, reallocation, and other fast paths are simpler. For example,
deallocation is just:

    static PAS_ALWAYS_INLINE void bmalloc_deallocate_inline(void* ptr)
    {
        pas_deallocate(ptr, BMALLOC_HEAP_CONFIG);
    }

If you look at `pas_deallocate`, you'll see cleverness that ensures that the slow path call is a tail call,
similarly to how allocators work. However, for deallocation, I haven't had the need to make the slow call
explicit in the client side (the way that `bmalloc_try_allocate_inline` has to explicitly call the slow path).

This concludes the discussion of libpas design.

# Testing Libpas

I've tried to write test cases for every behavior in libpas, to the point that you should feel comfortable
dropping a new libpas in WebKit (or wherever) if `test_pas` passes.

`test_pas` is a white-box component, regression, and unit test suite. It's allowed to call any libpas function,
even internal functions, and sometimes functions that libpas exposes only for the test suite.

Libpas testing errs on the side of being comprehensive even if this creates annoying situations. Many tests
assert detailed things about how many objects fit in a page, what an object's offset is in a page, and things
like that. This means that some behavior changes in libpas that aren't in any way wrong will set off errors in
the test suite. So, it's common to have to rebase tests when making libpas changes.

The libpas test suite is written in C++ and uses its own test harness that forks for each test, so each test
runs in a totally pristine state. Also, the test suite can use `malloc` and `new` as much as it likes, since in
the test suite, libpas does not replace `malloc` and `free`.

The most important libpas tests are the so-called *chaos* tests, which randomly create and destroy objects and
assert that the heap's state is still sane (like that no live objects overlap, that all live objects are
enumerable, etc).

# Conclusion

Libpas is a beast of a malloc, designed for speed, memory efficiency, and type safety. May whoever maintains it
find some joy in this insane codebase!

