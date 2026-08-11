import * as assert from '../assert.js';

function testI64(input, expected) {
    let global = new WebAssembly.Global({ value: 'i64' }, input);
    assert.eq(global.value, expected);
}

testI64(0n, 0n);
testI64(0x7fffffffffffffffn, 0x7fffffffffffffffn);
testI64(0x8000000000000000n, -0x8000000000000000n);
testI64(-1n, -1n);
testI64(-0x80000000n, -0x80000000n);
testI64(-0x7fffffffffffffffn, -0x7fffffffffffffffn);
testI64(-0x8000000000000000n, -0x8000000000000000n);
testI64(-0x8000000000000001n, 0x7fffffffffffffffn);
testI64(-0xffffffffffffffffn, 1n);
