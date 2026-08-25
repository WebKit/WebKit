---
name: gtest-parallel
description: Run Google Test binaries in parallel using the gtest-parallel script. Use when needing to speed up test execution, run flaky tests with repeat, or filter specific tests.
---

# gtest-parallel

`gtest-parallel` is a script that executes Google Test binaries in parallel,
providing speedup for single-threaded tests and tests that do not run at 100%
CPU.

## Location

The script is located at `third_party/gtest-parallel/gtest-parallel`.

## Core Flags

### Filtering Tests

Use `--gtest_filter` to run a select set of tests. It supports the same syntax
as Google Test (including exclusion with `-`).

```bash
third_party/gtest-parallel/gtest-parallel path/to/binary --gtest_filter=Foo.*:Bar.*
```

### Timeouts

- `--timeout=TIMEOUT`: Interrupt all remaining processes after the given time
  (in seconds).
- `--timeout_per_test=TIMEOUT_PER_TEST`: Interrupt single processes after the
  given time (in seconds).

### Output and Logging

- `-d OUTPUT_DIR`, `--output_dir=OUTPUT_DIR`: Output directory for test logs.
  Logs will be available under `gtest-parallel-logs/` inside the specified
  directory.
- `--dump_json_test_results=DUMP_JSON_TEST_RESULTS`: Saves the results of the
  tests as a JSON machine-readable file.

## Advanced Usage

### Repeating Tests (Flakiness Testing)

Use `--repeat=N` to run tests multiple times.

```bash
third_party/gtest-parallel/gtest-parallel path/to/binary --repeat=1000
```

### Workers

Use `-w WORKERS` or `--workers=WORKERS` to specify the number of parallel
workers (defaults to the number of cores).

### Serializing Test Cases

Use `--serialize_test_cases` to run tests within the same test case sequentially
(useful if they share resources).
