# JSTests

## Running Tests

Tests are not always set up to run directly. Instead run them through `Tools/Scripts/run-javascriptcore-tests`, which runs everything, or `Tools/Scripts/run-jsc-stress-tests <collection>`, which runs a specific subset. For day-to-day development, the `JSTests/stress` and `JSTests/wasm.yaml` collections are sufficient for catching most bugs.

Tests are run in a variety of different JSC configurations, for example with various JIT tiers disabled, concurrent compilation off, or the GC running continuously. These configurations show up as a suffix on the test name. For example, `stress/array-push.js.ftl-eager-no-cjit` corresponds to `JSTests/stress/array-push.js` with tier-up thresholds lowered and concurrent JIT off.

`run-jsc-stress-tests` looks for a `jsc` in the release build directory unless `--debug` or `--jsc <path>` is passed. `run-javascriptcore-tests` builds one first unless `--no-build` or `--root <path>` is passed.

`run-jsc-stress-tests` takes a collection (a directory or a `.yaml` file), not a single test file. To run a limited subset, pass `--filter <regex>`, which will pattern match on the test's name/configuration. e.g. `run-jsc-stress-tests JSTests/stress --filter array-push`.

## Adding Tests

Put tests that only target JS behavior in `JSTests/stress` and tests that involve wasm in `JSTests/wasm/stress`.

New tests are *required* to adhere to the following rules:

1. Tests must run in under 200ms in all configurations. This can be checked by passing `--report-execution-time` to `run-jsc-stress-tests`.
2. Use `testLoopCount` or `wasmTestLoopCount` to control how many iterations a test runs. The `jsc` CLI sets these based on the configuration of the test, so tests iterate enough to tier up where that matters and exit early where it doesn't.
3. Tests fail by crashing or throwing an uncaught exception, so assertions must throw rather than print. Add `//@ mustCrash!` or `//@ requireOptions("--exception=<exception-name>")` to the top of the test file if testing expected crashes/exceptions, respectively.
4. Don't print or log unless a test is about to fail. Test output goes straight to the terminal, so extra logging is noisy and disruptive.
5. Make sure the test actually reproduces the bug. Run the test against a build without the fix and make sure it fails.
