# Dart flute benchmark application

This is a WasmGC build of the `flute` benchmark, a Dart application using the
`Flutter` UI framework.
It performs the Wasm/compute-only part of rendering a certain number of frames.
Since JetStream runs in JS engine shells, the actual rendering to a Canvas is
stubbed out.
The upstream repository containing pre-built WebAssembly binaries (which we
use here) is at https://github.com/mkustermann/wasm_gc_benchmarks.
The Dart source code of the flute application is at https://github.com/dart-lang/flute.

## Build Instructions

See `build.sh` or just run it.
See `build.log` for the last build time, used sources, and toolchain versions.

## Running in JS shells

To run the unmodified upstream benchmark, without the JetStream driver, see the
upstream repo.
In short, the main runner is `build/run_wasm.js`, which takes as arguments a
application-specific generated JS and Wasm file, and the arguments passed to
the Dart main method.
Since different engines / shells resolve JS modules and parse command-line
arguments differently, the invocations are something like (from this directory):

```
path/to/d8 build/run_wasm.js -- flute.dart2wasm.mjs build/flute.dart2wasm.wasm -- $(date +%s.%N) 1000
path/to/spidermonkey/js build/run_wasm.js build/flute.dart2wasm.mjs flute.dart2wasm.wasm -- $(date +%s.%N) 1000
path/to/jsc build/run_wasm.js -- ./flute.dart2wasm.mjs build/flute.dart2wasm.wasm -- $(date +%s.%N) 1000
```
