# What is JetStream?

JetStream 3.0 is a JavaScript and WebAssembly benchmark suite.
For more information see the index and in-depth pages of the deployed benchmark.

<img src="./resources/screenshot.png">

## Open Governance

See [Governance.md](Governance.md) for more information.

## Getting Started, Setup Instructions

- Install Node.js and (optionally) [jsvu](https://github.com/GoogleChromeLabs/jsvu) for conveniently getting recent builds of engine shells.
- `npm install` the necessary dependencies.
- `npm run server` for starting a local development server, then browse to http://localhost:8010.
- `npm run test:shell` for running the benchmark in engine shells, or alternatively running directly, e.g., via `jsc cli.js`.

See [package.json](package.json) and [.github/workflows/test.yml](.github/workflows/test.yml) for more details and available commands.

### Shell Runner

For the shell runner, see the available options by passing `--help` to `cli.js`. (Note that this requires `--` for JavaScriptCore and V8 to separate VM arguments from script arguments.):

```
$ v8 cli.js -- --help
JetStream Driver Help

Options:
    --help                 Print this help message.
    --iteration-count      Set the default iteration count.
...

Available tags:
   all
...

Available tests:
   8bitbench-wasm
...
```

### Browser Runner

The browser version also supports passing parameters as URL query parameters, e.g., pass the `test` parameter (aliases are `tests` or `testList`) with a comma-separated list to run only specific workloads: [https://webkit-jetstream-preview.netlify.app/?test=8bitbench-wasm,web-ssr](https://webkit-jetstream-preview.netlify.app/?test=8bitbench-wasm,web-ssr).
See [utils/params.js](utils/params.js) and [JetStreamDriver.js](JetStreamDriver.js) for more details.

## Technical Details

The main file of the benchmark harness is `JetStreamDriver.js`, which lists the individual workloads and their parameters, implements measurement and scoring, etc.
The individual workloads are in subdirectories.

### Preloading and Compression

**Network prefetching (in the browser).**
In order to avoid the CPU frequency spinning down between tests we prefetch all assets before any of the tests start in the browser.
(In the CLI/shell runner we assume all assets are on disk.)
Assets are saved in a blob URL so they can be cached on disk.
This lowers the peak memory footprint of the benchmark to a sustainable level.

**Large asset preloading.**
The JetStream driver (both in the browser and shell runners) preloads some large assets and source files.
This avoids extensive disk I/O during the measurement window of the workloads.

**Compression.**
In order to limit the repository size and network transfers, large assets (e.g., ML models, heavy JavaScript bundles in the order of 10s of MBs) are stored as compressed .z files on disk.
Preloading handles the decompression of these assets (using `DecompressionStream` or a Wasm-based zlib polyfill) upfront so that decompression overhead does not affect the benchmark score.

Preloading, prefetching, and compression can be disabled, e.g., to inspect raw files or because it sometimes helps with debugging (e.g., to get proper URLs instead of blob URLs for resources).

- Compression: Run `npm run decompress` to decompress all .z files before running the benchmark.
- No prefetching for shells: Pass the `--no-prefetch` flag, e.g., `jsc cli.js -- --no-prefetch`.
- No prefetching in browsers: Append the query parameter `?prefetchResources=false` to the URL.

See `JetStreamDriver.js` and `utils/compress.mjs` for more details.

### Score Calculation

Scores in JetStream are dimensionless floats, where a higher score is better.
When scores are aggregated (e.g., multiple sub-scores for each workload, or to determine the total score of the full benchmark suite), we use the [geometric mean](https://en.wikipedia.org/wiki/Geometric_mean).
The geometric mean ensures that a multiplicative improvement of any individual score has the same effect on the aggregated score, regardless of the absolute value of the individual score.
For example, an improvement by 5% of the sub score of benchmark A has the same effect on the total score as an improvement by 5% of the sub score of benchmark B.

See the [in-depth.html](https://webkit-jetstream-preview.netlify.app/in-depth.html) and `JetStreamDriver.js` for more details.