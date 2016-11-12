import LowLevelBinary, * as LLB from '../LowLevelBinary.js';
import * as assert from '../assert.js';

let values = [];
for (let i = LLB.varint32Min; i !== LLB.varint32Min + 1024; ++i) values.push(i);
for (let i = -2048; i !== 2048; ++i) values.push(i);
for (let i = LLB.varint32Max; i !== LLB.varint32Max - 1024; --i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.varint32(i);
    const v = b.getVarint32(0);
    if (v.value !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
    if (v.next !== b.getSize())
        throw new Error(`Size ${v.next}, expected ${b.getSize()}`);
}

for (let i = 0; i < LLB.varBitsMax + 1; ++i) {
    let b = new LowLevelBinary();
    for (let j = 0; j < i; ++j)
        b.uint8(0x80);
    assert.throws(() => b.getVarint32(0), RangeError, `[${i}, ${i+1}) is out of buffer range [0, ${i})`);
}

let b = new LowLevelBinary();
for (let i = 0; i < LLB.varBitsMax; ++i)
    b.uint8(0x80);
b.uint8(0x00);
assert.throws(() => b.getVarint32(0), RangeError, `Shifting too much at 6`);
