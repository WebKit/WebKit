//@ skip if $memoryLimited
//@ requireOptions("--useDollarVM=1")

import * as assert from "../assert.js"

async function main() {
    let bytes = read('initialize-100k-functions.wasm', 'binary');
    await WebAssembly.compile(bytes).then((module) => {
        var peakFootprint = MemoryFootprint().peak;
        if (peakFootprint > 100_000_000) {
            print("Peak memory footprint exceeds 100000000. Actual peak: " + peakFootprint);
            $vm.abort()
        }
        return 0;
    });
}

await assert.asyncTest(main());

