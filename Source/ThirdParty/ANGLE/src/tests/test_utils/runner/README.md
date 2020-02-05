# ANGLE Test Harness

The ANGLE test harness is a harness around GoogleTest that provides functionality similar to the
[Chromium test harness][BaseTest]. It features:

 * splitting a test set into shards
 * catching and reporting crashes and timeouts
 * outputting to the Chromium [JSON test results format][JSONFormat]
 * multi-process execution

## Command-Line Arguments

The ANGLE test harness accepts all standard GoogleTest arguments. The harness also accepts the
following additional command-line arguments:

 * `--shard-count` and `--shard-index` control the test sharding
 * `--bot-mode` enables multi-process execution and test batching
 * `--batch-size` limits the number of tests to run in each batch
 * `--batch-timeout` limits the amount of time spent in each batch
 * `--max-processes` limits the number of simuntaneous processes
 * `--test-timeout` limits the amount of time spent in each test
 * `--results-file` specifies a location for the JSON test result output
 * `--results-directory` specifies a directory to write test results to
 * `--filter-file` allows passing a larget `gtest_filter` via a file

As well as the custom command-line arguments we support a few standard GoogleTest arguments:

 * `gtest_filter` works as it normally does with GoogleTest
 * `gtest_also_run_disabled_tests` works as it normally does as well

Other GoogleTest arguments are not supported although they may work.

## Implementation Notes

 * The test harness only requires `angle_common` and `angle_util`.
 * It does not depend on any Chromium browser code. This allows us to compile on other non-Clang platforms.
 * It uses rapidjson to read and write JSON files.
 * Timeouts are detected via a watchdog thread.
 * Crashes are handled via ANGLE's test crash handling code.
 * Currently it does not entirely support Android or Fuchsia.
 * Test execution is not currently deterministic in multi-process mode.
 * We capture stdout to output test failure reasons.

See the source code for more details: [TestSuite.h](TestSuite.h) and [TestSuite.cpp](TestSuite.cpp).

## Potential Areas of Improvement

 * Deterministic test execution.
 * Using sockets to communicate with test children. Similar to dEQP's test harness.
 * Closer integration with ANGLE's test expectations and system config libraries.
 * Supporting a GoogleTest-free integration.

[BaseTest]: https://chromium.googlesource.com/chromium/src/+/refs/heads/master/base/test/
[JSONFormat]: https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md
