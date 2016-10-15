import LowLevelBinary, * as LLB from '../LowLevelBinary.js';
import * as assert from '../assert.js';

let values = [];
for (let i = LLB.varintMin; i !== LLB.varintMin + 1024; ++i) values.push(i);
for (let i = -2048; i !== 2048; ++i) values.push(i);
for (let i = LLB.varintMax; i !== LLB.varintMax - 1024; --i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.varint(i);
    const v = b.getVarint(0);
    if (v.value !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
    if (v.next !== b.getSize())
        throw new Error(`Size ${v.next}, expected ${b.getSize()}`);
}

for (let i = 0; i < LLB.varBitsMax + 1; ++i) {
    let b = new LowLevelBinary();
    for (let j = 0; j < i; ++j)
        b.uint8(0x80);
    assert.throwsRangeError(() => b.getVarint(0));
}

let b = new LowLevelBinary();
for (let i = 0; i < LLB.varBitsMax; ++i)
    b.uint8(0x80);
b.uint8(0x00);
assert.throwsRangeError(() => b.getVarint(0));
