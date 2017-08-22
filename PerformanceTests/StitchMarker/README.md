# Stitch Marker: Threading Benchmarks

> In crochet, a stitch marker is a mnemonic device used to distinguish important
> locations on a work in progress. Crochet patterns have a mathematical basis,
> so stitch markers serve as a visual reference that takes the place of
> continuous stitch counting and reduces a crocheter's error rate.

Stitch Marker is a collection of threading benchmarks. It measures an abstract
machine's low-level capabilities for atomic instructions, higher-level waiting,
and general scalability under contention. It collects state-of-the-art
approaches to locking and lock-free code as well as older approaches, and a few
naive ones because in the real world everyone loses a stitch or two from time to
time. The point isn't to compare different lock implementations, or fancy
lock-free algorithms, it's rather to compare how they lower onto different
(virtual) ISAs.


# Benchmarks

## FMJS Litmus

* Author: Cristian Mattarei
* Benchmark: [FMJS EMME](https://github.com/FMJS/EMME) litmus tests
* Version: FIXME
* Note: FIXME once we have a version which can target WebAssembly

These tests define the boundary of correctness for the JavaScript
SharedArrayBuffer memory model, which WebAssembly mostly re-uses. The tests
themselves are probabilistic in that the programs are run and their outcome is
validated against a list of known-allowed outcomes. Running the tests many times
will, over time, find any implementation flaws.

Running these tests in a loop has attractive properties typical microbenchmarks
do not have: it validates that an implementation can perform as fast as possible
at the boundary of correctness, but no faster. By definition this microbenchmark
*cannot* be gamed because doing so out cause it to fail.


## folly

* Author: Facebook
* Repository: [folly](https://github.com/facebook/folly.git)
* Version: `45245cb293e3c61deb8b952956507b62e88b0cf9`

Facebook's folly library has multiple classes related to concurrency and
parallelism, and benchmark many of them.


## ogiroux

* Author: Olivier Giroux
* Benchmark: [Semaphore C++ standard proposal](https://github.com/ogiroux/semaphore)
* Version: `8b2c4ea06952084913ae18d6bb66b0a957d96cd4`

Code and benchmark for a C++ standards committee proposal adding a
high-performance semaphore class to C++.


## ck

* Author: Samy Al Bahra
* Benchmark: [concurrency kit](https://github.com/concurrencykit/ck)
* Version: `0d1e86d18ee58534bea976489276d75053279100`

Concurrency primitives, safe memory reclamation mechanisms and non-blocking data
structures for the research, design and implementation of high performance
concurrent systems.


## WTF

* Author: Filip Jerzy Pizło
* benchmark: [WebKit Template Framework lock benchmarks](https://trac.webkit.org/browser/webkit/trunk/Source/WTF/benchmarks)
* Version: `r220151`
