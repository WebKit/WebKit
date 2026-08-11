# Test262 Tools

Tooling for running the [Test262](https://github.com/tc39/test262) conformance suite
against JavaScriptCore and for importing new versions of the suite into the tree.

## Runner script

To run the suite, invoke the runner:

```sh
./Tools/Scripts/test262-runner --release
```

The build configuration defaults to `--release`; pass `--debug` to use a debug build
instead (the two are mutually exclusive). The runner discovers the matching `jsc` binary
under `WebKitBuild` by default; pass `--jsc <path>` to use a specific binary instead.

By default, every test is run, results are compared against the checked-in
expectations file and the script exits non-zero if any new failures appear
(failures already listed in the expectations file are ignored).
Reports are written to `test262-results/` in the current directory.

### Common options

If you need to customize the execution, check out `./Tools/Scripts/test262-runner --help` for extra commands.

| Option                           | Description                                                              |
| -------------------------------- | ------------------------------------------------------------------------ |
| `--release` / `--debug`          | Which build of `jsc` to run (defaults to `--release`).                   |
| `-j, --jsc <path>`               | Use a specific `jsc` binary.                                             |
| `--gtk` / `--wpe` / `--jsc-only` | Find `jsc` under the given port's build directory.                       |
| `-o, --test-only <path>`         | Run only a subdirectory or single test file (repeatable).                |
| `--filter <regex>`               | Run only tests whose path matches the regex.                             |
| `-f, --features <feature>`       | Run only tests with the given feature (repeatable).                      |
| `-p, --child-processes <n>`      | Number of parallel worker processes.                                     |
| `--timeout <ms>`                 | Per-test timeout in milliseconds.                                        |
| `-v, --verbose`                  | Print each test's result and a new failure/pass summary.                 |
| `--save`                         | Save current failures to the expectations file.                          |
| `-x, --ignore-expectations`      | Report all failures, not just new ones.                                  |
| `-i, --ignore-config`            | Ignore `config.yaml`.                                                    |
| `-l, --latest-import`            | Run only the files changed by the last import.                           |
| `-F, --failing-files`            | Re-run only the tests that failed in a previous results file.            |
| `-S, --skipped-files`            | Run only the tests that the config skips.                                |
| `--stats`                        | Recompute summaries from an existing results file without running tests. |
| `--sanitizer <name>`             | Declare the sanitizer in use (e.g. `asan`) for conditional skips.        |

### [JSTests/test262/config.yaml](https://github.com/WebKit/WebKit/blob/main/JSTests/test262/config.yaml)

This file controls which tests are skipped and which JSC runtime flags to enable
for particular features. Example:

```yaml
---
# Enable a JSC feature flag for every test that lists the given feature.
# Adds `--<flag>=1` to the jsc command line.
flags:
  SharedArrayBuffer: useSharedArrayBuffer
  Temporal: useTemporal

skip:
  # Skip any test whose metadata lists one of these features.
  features:
    - decorators
    - source-phase-imports
  # Skip tests whose path matches one of these regexes.
  paths:
    - test/staging/Intl402
  # Skip these individual test files.
  files:
    - test/built-ins/Array/prototype/reverse/length-exceeding-integer-limit-with-object.js
  # Skip only under matching build/platform conditions. Each entry's `if`
  # clause may constrain os, os_version, build (release/debug) and sanitizer;
  # files/paths/features are skipped only when the condition matches.
  conditions:
    - if:
        os: macOS
        os_version: 26.4
      features:
        - Atomics
    - if:
        build: debug
      paths:
        - test/built-ins/RegExp/property-escapes
```

### [JSTests/test262/expectations.yaml](https://github.com/WebKit/WebKit/blob/main/JSTests/test262/expectations.yaml)

This file records every expected failure. When JSC or Test262 changes, it should
be updated so that developers see only the failures they introduce.

Each entry maps a test file to the modes it's expected to fail in (`default`,
`strict mode`, `module` or `raw`), and each mode to the exit code `jsc` is
expected to terminate with:

- `3`: the test ran to completion and failed (a failed assertion or an uncaught exception; JSC's `EXIT_EXCEPTION`).
- `128 + N`: `jsc` was killed by signal `N`, i.e. it crashed (e.g. `134` SIGABRT, `139` SIGSEGV).
- `1`: `jsc` could not run the test at all, or the runner timed out.

Because the recorded exit code has to match, a test that starts crashing where
it previously failed an assertion is still reported as a new failure.

On Linux, the runner uses [expectations-linux.yaml](https://github.com/WebKit/WebKit/blob/main/JSTests/test262/expectations-linux.yaml)
instead.

To update the expectations file, run:

```sh
./Tools/Scripts/test262-runner --release --save
```

`--save` updates entries incrementally: if you run a subset of the suite (for
example with `--test-only` or `--filter`), only the tests that actually ran are
rewritten and the rest of the file is left untouched.

### test262-results/

Written to the current directory on every run:

- `results.yaml`: results for every test that ran.
- `summary.txt`, `summary.yaml`, `summary.html`: pass/fail/skip statistics by feature and by folder.
- `index.html`: an expandable report of the failures (styled by `report.css`).

## Import script

To import a new version of Test262, run the importer. With no arguments,
it shallow-clones the canonical upstream repository and updates the
`JSTests/test262` folder:

```sh
./Tools/Scripts/test262-import
```

It records the imported commit in `JSTests/test262/test262-Revision.txt`
and a list of the changed files in `JSTests/test262/latest-changes-summary.txt`
(which the runner's `--latest-import` mode reads). If the revision has not
changed since the last import, it exits without doing anything.

### Options

Run `./Tools/Scripts/test262-import --help` for details.

| Option                | Description                                                                   |
| --------------------- | ----------------------------------------------------------------------------- |
| `-s, --src <path>`    | Import from a local Test262 checkout instead of cloning.                      |
| `-r, --remote <url>`  | Clone from a specific remote (default `https://github.com/tc39/test262.git`). |
| `-b, --branch <name>` | Branch to clone from a remote (default `main`).                               |

`--src` and `--remote` are mutually exclusive. A local source must be a clean
Git checkout containing `test/` and `harness/` directories.

## Development

Both tools require Python 3 and [PyYAML](https://pyyaml.org/)
(`pip3 install pyyaml`). They rely on `webkitcorepy` and `webkitpy`, which
live in the tree under `Tools/Scripts`, so no other setup is needed.

The runner and importer live in `Tools/Scripts/test262/` (`runner.py` and
`importer.py`); the top-level `test262-runner` and `test262-import` scripts are
thin wrappers around them.

### Running the unit tests

The `test262` tree is registered with `test-webkitpy`:

```sh
./Tools/Scripts/test-webkitpy test262
```

Or run a single module directly:

```sh
cd Tools/Scripts/test262
python3 -m unittest runner_unittest importer_unittest
```
