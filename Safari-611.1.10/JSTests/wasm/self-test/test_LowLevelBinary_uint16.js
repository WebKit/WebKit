import LowLevelBinary from '../LowLevelBinary.js';

let values = [];
for (let i = 0; i <= 0xFFFF; ++i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.uint16(i);
    const v = b.getUint16(0);
    if (v !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
}
