/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as fs from 'fs';
import * as http from 'http';


import { dataCache } from '../framework/data_cache.js';
import { globalTestConfig } from '../framework/test_config.js';
import { DefaultTestFileLoader } from '../internal/file_loader.js';
import { prettyPrintLog } from '../internal/logging/log_message.js';
import { Logger } from '../internal/logging/logger.js';

import { parseQuery } from '../internal/query/parseQuery.js';


import { Colors } from '../util/colors.js';
import { setGPUProvider } from '../util/navigator_gpu.js';

import sys from './helper/sys.js';

function usage(rc) {
  console.log(`Usage:
  tools/run_${sys.type} [OPTIONS...]
Options:
  --colors                  Enable ANSI colors in output.
  --coverage                Add coverage data to each result.
  --data                    Path to the data cache directory.
  --verbose                 Print result/log of every test as it runs.
  --gpu-provider            Path to node module that provides the GPU implementation.
  --gpu-provider-flag       Flag to set on the gpu-provider as <flag>=<value>
  --unroll-const-eval-loops Unrolls loops in constant-evaluation shader execution tests
  --u                       Flag to set on the gpu-provider as <flag>=<value>

Provides an HTTP server used for running tests via an HTTP RPC interface
To run a test, perform an HTTP GET or POST at the URL:
  http://localhost:port/run?<test-name>
To shutdown the server perform an HTTP GET or POST at the URL:
  http://localhost:port/terminate
`);
  return sys.exit(rc);
}



























if (!sys.existsSync('src/common/runtime/cmdline.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

Colors.enabled = false;

let emitCoverage = false;
let verbose = false;
let gpuProviderModule = undefined;
let dataPath = undefined;

const gpuProviderFlags = [];
for (let i = 0; i < sys.args.length; ++i) {
  const a = sys.args[i];
  if (a.startsWith('-')) {
    if (a === '--colors') {
      Colors.enabled = true;
    } else if (a === '--coverage') {
      emitCoverage = true;
    } else if (a === '--data') {
      dataPath = sys.args[++i];
    } else if (a === '--gpu-provider') {
      const modulePath = sys.args[++i];
      gpuProviderModule = require(modulePath);
    } else if (a === '--gpu-provider-flag') {
      gpuProviderFlags.push(sys.args[++i]);
    } else if (a === '--unroll-const-eval-loops') {
      globalTestConfig.unrollConstEvalLoops = true;
    } else if (a === '--help') {
      usage(1);
    } else if (a === '--verbose') {
      verbose = true;
    } else {
      console.log(`unrecognised flag: ${a}`);
    }
  }
}

let codeCoverage = undefined;

if (gpuProviderModule) {
  setGPUProvider(() => gpuProviderModule.create(gpuProviderFlags));

  if (emitCoverage) {
    codeCoverage = gpuProviderModule.coverage;
    if (codeCoverage === undefined) {
      console.error(
      `--coverage specified, but the GPUProviderModule does not support code coverage.
Did you remember to build with code coverage instrumentation enabled?`);

      sys.exit(1);
    }
  }
}

if (dataPath !== undefined) {
  dataCache.setStore({
    load: (path) => {
      return new Promise((resolve, reject) => {
        fs.readFile(`${dataPath}/${path}`, 'utf8', (err, data) => {
          if (err !== null) {
            reject(err.message);
          } else {
            resolve(data);
          }
        });
      });
    }
  });
}
if (verbose) {
  dataCache.setDebugLogger(console.log);
}


(async () => {
  Logger.globalDebugMode = verbose;
  const log = new Logger();
  const testcases = new Map();

  async function runTestcase(
  testcase,
  expectations = [])
  {
    const name = testcase.query.toString();
    const [rec, res] = log.record(name);
    await testcase.run(rec, expectations);
    return res;
  }

  const server = http.createServer(
  async (request, response) => {
    if (request.url === undefined) {
      response.end('invalid url');
      return;
    }

    const loadCasesPrefix = '/load?';
    const runPrefix = '/run?';
    const terminatePrefix = '/terminate';

    if (request.url.startsWith(loadCasesPrefix)) {
      const query = request.url.substr(loadCasesPrefix.length);
      try {
        const webgpuQuery = parseQuery(query);
        const loader = new DefaultTestFileLoader();
        for (const testcase of await loader.loadCases(webgpuQuery)) {
          testcases.set(testcase.query.toString(), testcase);
        }
        response.statusCode = 200;
        response.end();
      } catch (err) {
        response.statusCode = 500;
        response.end(`load failed with error: ${err}\n${err.stack}`);
      }
    } else if (request.url.startsWith(runPrefix)) {
      const name = request.url.substr(runPrefix.length);
      try {
        const testcase = testcases.get(name);
        if (testcase) {
          if (codeCoverage !== undefined) {
            codeCoverage.begin();
          }
          const result = await runTestcase(testcase);
          const coverageData = codeCoverage !== undefined ? codeCoverage.end() : undefined;
          let message = '';
          if (result.logs !== undefined) {
            message = result.logs.map((log) => prettyPrintLog(log)).join('\n');
          }
          const status = result.status;
          const res = { status, message, coverageData };
          response.statusCode = 200;
          response.end(JSON.stringify(res));
        } else {
          response.statusCode = 404;
          response.end(`test case '${name}' not found`);
        }
      } catch (err) {
        response.statusCode = 500;
        response.end(`run failed with error: ${err}`);
      }
    } else if (request.url.startsWith(terminatePrefix)) {
      server.close();
      sys.exit(1);
    } else {
      response.statusCode = 404;
      response.end('unhandled url request');
    }
  });


  server.listen(0, () => {
    const address = server.address();
    console.log(`Server listening at [[${address.port}]]`);
  });
})().catch((ex) => {
  console.error(ex.stack ?? ex.toString());
  sys.exit(1);
});
//# sourceMappingURL=server.js.map