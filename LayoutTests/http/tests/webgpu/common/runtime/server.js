/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as http from 'http';


import { DefaultTestFileLoader } from '../internal/file_loader.js';
import { prettyPrintLog } from '../internal/logging/log_message.js';
import { Logger } from '../internal/logging/logger.js';

import { parseQuery } from '../internal/query/parseQuery.js';


import { setGPUProvider } from '../util/navigator_gpu.js';

import sys from './helper/sys.js';

function usage(rc) {
  console.log('Usage:');
  console.log(`  tools/run_${sys.type} [OPTIONS...]`);
  console.log('Options:');
  console.log('  --verbose            Print result/log of every test as it runs.');
  console.log('  --gpu-provider       Path to node module that provides the GPU implementation.');
  console.log('  --gpu-provider-flag  Flag to set on the gpu-provider as <flag>=<value>');
  console.log(``);
  console.log(`Provides an HTTP server used for running tests via an HTTP RPC interface`);
  console.log(`To run a test, perform an HTTP GET or POST at the URL:`);
  console.log(`  http://localhost:port/run?<test-name>`);
  console.log(`To shutdown the server perform an HTTP GET or POST at the URL:`);
  console.log(`  http://localhost:port/terminate`);
  return sys.exit(rc);
}










if (!sys.existsSync('src/common/runtime/cmdline.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

let debug = false;
let gpuProviderModule = undefined;

const gpuProviderFlags = [];
for (let i = 0; i < sys.args.length; ++i) {
  const a = sys.args[i];
  if (a.startsWith('-')) {
    if (a === '--gpu-provider') {
      const modulePath = sys.args[++i];
      gpuProviderModule = require(modulePath);
    } else if (a === '--gpu-provider-flag') {
      gpuProviderFlags.push(sys.args[++i]);
    } else if (a === '--help') {
      usage(1);
    } else if (a === '--verbose') {
      debug = true;
    }
  }
}

if (gpuProviderModule) {
  setGPUProvider(() => gpuProviderModule.create(gpuProviderFlags));
}

(async () => {
  Logger.globalDebugMode = debug;
  const log = new Logger();
  const testcases = allWebGPUTestcases();

  async function allWebGPUTestcases() {
    const webgpuQuery = parseQuery('webgpu:*');
    const loader = new DefaultTestFileLoader();
    const map = new Map();
    for (const testcase of await loader.loadCases(webgpuQuery)) {
      const name = testcase.query.toString();
      map.set(name, testcase);
    }
    return map;
  }

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

    const runPrefix = '/run?';
    const terminatePrefix = '/terminate';

    if (request.url.startsWith(runPrefix)) {
      const name = request.url.substr(runPrefix.length);
      try {
        const testcase = (await testcases).get(name);
        if (testcase) {
          const result = await runTestcase(testcase);
          let message = '';
          if (result.logs !== undefined) {
            message = result.logs.map((log) => prettyPrintLog(log)).join('\n');
          }
          const status = result.status;
          const res = { status, message };
          response.statusCode = 200;
          response.end(JSON.stringify(res));
        } else {
          response.statusCode = 404;
          response.end(`test case '${name}' not found`);
        }
      } catch (err) {
        response.statusCode = 500;
        response.end('run failed with error: ' + err);
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