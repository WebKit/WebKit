import LowLevelBinary, * as LLB from '../LowLevelBinary.js';

let values = [];
for (let i = 0; i <= 1; ++i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.varuint1(i);
    const v = b.getVaruint1(0);
    if (v.value !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
    if (v.next !== b.getSize())
        throw new Error(`Size ${v.next}, expected ${b.getSize()}`);
}
