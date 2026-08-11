# Test262 Import and Runner

## Summary

Test262 is a project maintained by Ecma\'s TC39 with conformance tests
for ECMA-262, ECMA-402 and ECMA-404, covering the language syntax and
built-in APIs. It\'s entirely written in JS.

In a local copy of the WebKit source...

Test262 is imported to the `./JSTests/test262` folder; helper files can be
found in `./JSTests/test262/harness` and the actual test files are in
`./JSTests/test262/test`.

The Test262 import and runner scripts (`test262-import` and
`test262-runner`) are located in the `./Tools/Scripts` folder, with
dependencies located in `./Tools/Scripts/test262`.

To update WebKit\'s local Test262, execute
`./Tools/Scripts/test262-import` (if `./Tools/Scripts` is in the env PATH,
execute `test262-import`).

To run WebKit\'s local Test262, execute `./Tools/Scripts/test262-runner`
(if `./Tools/Scripts` is in the env PATH, execute `test262-runner``).

## test262-import

The import script will fetch the master branch of Test262, published to
the official repository at [https://github.com/tc39/test262](https://github.com/tc39/test262).
The changes are applied in the `./JSTests/test262` folder, along with
additional information:

- `test262-Revision.txt` will store the latest import revision (the commit
  hash) and the source from the last import. This information is used to
  compare changes in further imports. Example:

```txt
> test262 remote url: git@github.com:tc39/test262.git
>
> test262 revision: 7dc92154af01c6772b1a773e1e9fcb706b863de0
```

- `latest-changes-summary.txt` will store a summary of the latest imported
  files, including status codes: (A) added, (M) modified, (R) renamed
  and (D) deleted files. This information is also useful to the runner
  if the user wants to check only the newly imported files.

Although it\'s not recommended, Test262 can be imported from a local
folder, using the \--src argument, ie. test262-import \--src \<folder\>.
The script can also import from a custom remote git source, ie.
test262-import \--remote \<url\>.

```
test262-import

Settings:

Remote: git@github.com:tc39/test262.git

Branch: master

--------------------------------------------------------

Importing Test262 from git

\> git clone -b master \--depth=1 git@github.com:tc39/test262.git
/var/folders/dl/8cvkdgx50mbdyr18zl7lrnkw0000gn/T/w3s9oQT73d

Cloning into
\'/var/folders/dl/8cvkdgx50mbdyr18zl7lrnkw0000gn/T/w3s9oQT73d\'\...

remote: Counting objects: 33660, done.

remote: Compressing objects: 100% (13067/13067), done.

remote: Total 33660 (delta 21915), reused 25337 (delta 20553),
pack-reused 0

Receiving objects: 100% (33660/33660), 8.84 MiB \| 4.90 MiB/s, done.

Resolving deltas: 100% (21915/21915), done.

Checking out files: 100% (32667/32667), done.

New tracking: git@github.com:tc39/test262.git

From branch: master

New revision: 7dc92154af01c6772b1a773e1e9fcb706b863de0

Summary of changes:

D test/built-ins/RegExp/S15.10.2.12_A2_T1.js

A test/built-ins/RegExp/character-class-escape-non-whitespace-u180e.js

A test/built-ins/RegExp/character-class-escape-non-whitespace.js

M
test/built-ins/RegExp/property-escapes/unsupported-binary-properties.js

M test/built-ins/global/global-object.js

M test/built-ins/global/property-descriptor.js

> rm -rf /Users/leo/dev/webkit/JSTests/test262/harness

> rm -rf /Users/leo/dev/webkit/JSTests/test262/test

> mv
/var/folders/dl/8cvkdgx50mbdyr18zl7lrnkw0000gn/T/w3s9oQT73d/harness
/Users/leo/dev/webkit/JSTests/test262

> mv /var/folders/dl/8cvkdgx50mbdyr18zl7lrnkw0000gn/T/w3s9oQT73d/test
/Users/leo/dev/webkit/JSTests/test262

Done in 37 seconds!
```

After updating Test262, a new commit is necessary. Running
`test262-runner` is also necessary to check for any new test results.

## test262-runner

When called with no arguments, this script will run all the tests from
Test262, with the exception of those files included in the skip list.
The skip list is defined in `./JSTests/test262/config.yaml`. **Changes in
this file are not automated.** A human must add or remove tests to be
skipped. The skip list is also documentation of tests which are
failures, mostly due to known---and linked---bugs or new features not
yet implemented in JavaScriptCore. The skip list can list test files by
their path or using features tags, which correspond to metadata defined
in each of the Test262 test files. Executing `test262-runner
−−skipped−files` will run all of the skipped tests and flag any newly
passing tests.

Additional options and flags for test262-runner can be found by
executing `test262-runner --help`.

When executed, the runner will read from
`./JSTests/test262/expectations.yaml` a list with tests files that are
expected to fail and their latest reported failure. If any new failure
is found, the runner will report them as new failures and will close the
program with a non-zero exit code.

```

test262-runner -o test/built-ins/ArrayBuffer

Settings:

Test262 Dir: JSTests/test262

JSC: ../../.jsvu/jsc

Child Processes: 32

Paths: test/built-ins/ArrayBuffer

Config file: JSTests/test262/config.yaml

Expectations file: JSTests/test262/expectations.yaml

---

! NEW FAIL test/built-ins/ArrayBuffer/init-zero.js (strict mode)

Exception: Test262Error: Expected SameValue(«1», «2») to be true

! NEW FAIL test/built-ins/ArrayBuffer/init-zero.js (default)

Exception: Test262Error: Expected SameValue(«1», «2») to be true

git

156 tests run

2 test files skipped

24 tests failed in total

2 tests newly fail

0 tests newly pass

Saved all the results in
/Users/leo/dev/webkit/test262-results/results.yaml

Summarizing results\...

See the summaries and results in the
/Users/leo/dev/webkit/test262-results.

Done in 3.42 seconds!

```

With new changes from the JavaScriptCore source or with new updates from
Test262, it\'s important to record these files in the skip list, or as
new failures in the expectations file. This can be done by calling
`test262-runner --save` and commit the changes.

The expectations file is a machine generated file that doesn\'t allow
tracking the reason or bugs referencing the failure, e.g. an
un-implemented Stage 3 feature. It\'s recommended to triage new failures
and add them to the skip list with a matching Bugzilla link using a
comment line. Note that the expectations file exists to unblock any
updates of Test262 into WebKit.

To run a specific file or folder, call `test262-runner -o <path>`. This
option can be stacked to multiple paths: `test262-runner -o <path1> -o
<path2>`.

To triage new failures from a recent Test262 import, there is an option
to run only the recently added and modified test files: `test262-runner
−−latest−import`.

For a complete run, including the tests that would be skipped, call:
`test262-runner --ignore-config`.

By default, the `test262-runner` will try to detect the path for
JavaScriptCore, and it\'s also possible to provide a custom path calling
it with `test262-runner −−jsc <path-for-jsc>`, this will also try to set
the environment\'s `DYLD_FRAMEWORK_PATH` (if not yet defined). The default
JavaScriptCore path is detected in the following order, returning an
error if it doesn\'t find JavaScriptCore:

- The expected folder similar to calling `webkit-build-directory --debug`

- The expected folder similar to calling `webkit-build-directory`
  (release)

- A path found calling which jsc

By default, the `test262-runner` uses 4 child processes per core to
execute a test run. If the target machine has 4 cores available, it will
use 16 children processes. If only 1 core is available, it will use 4
processes. To set a custom number of cores, the runner should be called
as `test262-runner -p <number>` with the desired number of cores to be
used.

When the `test262-runner` is done running the tests, it creates a git
ignored folder which contains the summaries and reports output by the
latest run. This folder is named test262-results and is placed in the
current folder where the runner was called.

This `test262-results` folder may contain the following files:

- `index.html`: an HTML report with a short summary and list of failures.
  It includes all the failures, not only the new failures.

- `summary.html`: presenting two tables of summaries of the results per
  path - folders and subfolders - and features from the frontmatter
  metadata.

- `report.css`: used in both HTML files.

- `results.yaml`: a long Yaml file with all the results, can be consumed
  by any script.

- `summary.txt`: a text version of the summaries.

- `summary.yaml`: a Yaml file with the data used for the summaries

