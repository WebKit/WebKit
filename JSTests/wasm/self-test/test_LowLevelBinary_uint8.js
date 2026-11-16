import LowLevelBinary from '../LowLevelBinary.js';

let values = [];
for (let i = 0; i <= 0xFF; ++i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.uint8(i);
    const v = b.getUint8(0);
    if (v !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
}
