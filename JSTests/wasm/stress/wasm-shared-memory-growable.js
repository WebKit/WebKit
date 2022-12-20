//@ skip if $architecture == "arm"

import * as assert from "../assert.js";

{
    let memory = new WebAssembly.Memory({
        shared: true,
        initial: 1,
        maximum: 16,
    });

    let buffer = memory.buffer;
    assert.eq(buffer.growable, false);
    assert.eq(buffer.maxByteLength, 65536);
    assert.eq(buffer.byteLength, 65536);
}
{
    let memory = new WebAssembly.Memory({
        shared: false,
        initial: 1,
    });

    let buffer = memory.buffer;
    assert.eq(buffer.resizable, false);
    assert.eq(buffer.maxByteLength, 65536);
    assert.eq(buffer.byteLength, 65536);
}
