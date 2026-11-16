# JSTests/microbenchmarks

Each file is a small, self-contained program that runs one engine path in a hot loop. They catch performance regressions and show whether an optimization actually helps. `JSTests/slowMicrobenchmarks` holds longer-running ones and follows the same rules.

## Running Benchmarks

Compare builds with `Tools/Scripts/run-jsc-benchmarks`, passing one `<name>:<path-to-jsc>` per build:

```
Tools/Scripts/run-jsc-benchmarks --microbenchmarks --benchmarks 'object-entries' \
    Baseline:/path/to/baseline/jsc Patched:/path/to/patched/jsc
```

- `--microbenchmarks` (or `--slow-microbenchmarks`) restricts the run to this directory. Without it, the script runs every suite it knows about.
- `--benchmarks <regex>` keeps only benchmarks whose `Microbenchmarks/<file-name>` matches. Without it, all ~1750 files run, which takes a long time.
- The baseline needs its own build: build without your change, copy the build directory somewhere else, then build with your change. The script sets `DYLD_FRAMEWORK_PATH` to the directory containing each `jsc`.
- Each benchmark runs in `--outer` separate `jsc` processes per build (default 4), in random order across builds. Each process loads the file with `run()` once to warm up, then once more for the timed iteration. Use something like `--outer 20` for differences of a few percent.
- For each benchmark, the report shows the mean time per build with a 95% confidence interval. "definitely" means the intervals don't overlap; "might be" means they do, so treat it as noise. The report and raw JSON go to the current directory.

`//@` directives are read only by `run-jsc-stress-tests`; `run-jsc-benchmarks` ignores them. If a benchmark needs an option (for example `//@ requireOptions("--useTemporal=1")`), pass it as an environment variable on *every* VM, since the script doesn't unset it between VMs: `Baseline:JSC_useTemporal=1:/path/to/baseline/jsc`.

To iterate quickly on one file, `time /path/to/jsc JSTests/microbenchmarks/<name>.js` works too, but that's a single sample and it includes process startup.

`run-javascriptcore-tests` also runs every file once, in the default configuration, as a correctness test (`JSTests/microbenchmarks.yaml`). To run that check by itself: `Tools/Scripts/run-jsc-stress-tests JSTests/microbenchmarks.yaml --filter <name>`.

## Adding Benchmarks

1. Measure one thing per file, and name the file after it (e.g. `object-entries-dense-indexed.js`). The file name is the benchmark name in reports.
2. The file must run under plain `jsc <file>` with no harness. jsc shell functions such as `noInline` are available.
3. Check results and throw if they're wrong. That keeps the optimizer from deleting the work being measured, and makes the file a real correctness test. Print nothing on success.
4. Do setup outside the timed loop so the path being measured dominates. Most files finish in tens of milliseconds on a release build. They also run in debug builds under `run-javascriptcore-tests`, so put anything much longer in `JSTests/slowMicrobenchmarks`.
5. Confirm the benchmark measures your change: compare builds with and without it, and expect a "definitely" result.
