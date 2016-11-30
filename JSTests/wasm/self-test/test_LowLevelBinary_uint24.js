import LowLevelBinary from '../LowLevelBinary.js';

let values = [];
for (let i = 0; i <= 0xFFFF; ++i) values.push(i);
for (let i = 0xFFFFFF - 0xFFFF; i <= 0xFFFFFF; ++i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.uint24(i);
    const v = b.getUint24(0);
    if (v !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
}
