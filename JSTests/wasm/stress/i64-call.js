/*

cd ../../../; ./Tools/Scripts/build-webkit --debug --jsc-only --fuse-ld=mold; cd JSTests/wasm/stress

export BUILDDIR=/home/igalia/jmichaud/WebKit && export VM=$BUILDDIR/WebKitBuild/JSCOnly/Debug/ && LD_LIBRARY_PATH=$VM $VM/bin/jsc --validateOptions=1 --useConcurrentJIT=0 -m --useBBQJIT=0 --jitPolicyScale=0 --useDFGJIT=0 --useOMGJIT=1 i64-call.js

export BUILDDIR=/home/igalia/jmichaud/WebKit && export VM=$BUILDDIR/WebKitBuild/JSCOnly/Debug/ && LD_LIBRARY_PATH=$VM gdb -ex="handle SIGUSR1 nostop noprint" -ex="handle SIGILL stop ignore" -ex="run" --args $VM/bin/jsc --validateOptions=1 --useConcurrentJIT=0 -m --useBBQJIT=0 --jitPolicyScale=0 --useDFGJIT=0 --useOMGJIT=1 i64-call.js

*/

import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param i64 i64 i64) (result i64)
    local.get 0
    local.get 1
    local.get 2
    i64.const 42
    i64.add
    i64.add
    i64.add
  )
)
`;

async function test() {
    let instance = await instantiate(wat);
    if (instance.exports.test(44n, 22n, -3n) !== 105n)
        throw new Error();
}

await assert.asyncTest(test());
