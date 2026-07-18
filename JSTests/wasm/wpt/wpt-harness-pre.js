// Bootstrap for running the imported WPT WebAssembly tests in the `jsc` shell under
// run-jsc-stress-tests. It is the first of three harness shims loaded around each test:
// wpt-harness-pre.js (this file) before resources/testharness.js (the imported WPT copy,
// LayoutTests/imported/w3c/web-platform-tests/resources/testharness.js — the version the browser
// loads for these tests, not LayoutTests/resources/testharness.js); wpt-harness-mid.js just before
// the test file (it drains microtasks so the WAST harness's deferred import registry is installed,
// and marks the run explicit_done); and wpt-harness-post.js after the test (it emits the results
// and calls done()).
//
// The jsc shell has no `self` (testharness.js invokes its IIFE as `})(self)`) and leaves
// `console` undefined, so both must be supplied here, before testharness.js loads. `console` is a
// no-op: the browser's testharnessreport.js does not dump console output into the test text, and
// the WAST harness binds the wasm `spectest.print` imports to `console.log` — suppressing it keeps
// that output out of stdout so it matches the committed -expected.txt baselines.
globalThis.self = globalThis;
globalThis.console = { log() {}, error() {}, warn() {}, info() {}, debug() {} };
