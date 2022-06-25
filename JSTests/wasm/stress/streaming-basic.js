import * as assert from '../assert.js';

var data = read("./nameSection.wasm", "binary");

function check(step)
{
    var parser = $vm.createWasmStreamingParser();
    for (var i = 0; i < data.byteLength; i += step) {
        parser.addBytes(data.subarray(i, i + Math.min(step, data.byteLength - i)));
    }
    assert.eq(parser.finalize(), 7);
}

for (var i = 0; i < 10; ++i)
    check(i + 1);
check(100);
check(1000);
check(2000);
