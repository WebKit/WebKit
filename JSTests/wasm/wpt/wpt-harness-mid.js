// Loaded after resources/testharness.js and the WPT harness helpers, immediately before the test
// file, when running the imported WPT WebAssembly tests in the `jsc` shell under
// run-jsc-stress-tests.
//
// A browser takes an event-loop turn between the harness <script> and the test <script>; the jsc
// shell load()s every file synchronously with no turn in between. The WAST core harness
// (core/js/harness/async_index.js) installs its `spectest` import registry Proxy in a deferred
// microtask (reinitializeRegistry -> chain.then), so without a turn here that Proxy is not yet in
// place when a test body runs.
//
// explicit_done keeps testharness from finalizing the run while we drain.
setup({ output: false, explicit_timeout: true, explicit_done: true });
drainMicrotasks();
