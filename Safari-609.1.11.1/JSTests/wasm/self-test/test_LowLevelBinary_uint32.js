import LowLevelBinary from '../LowLevelBinary.js';

let values = [];
for (let i = 0; i !== 0xFFFF + 1; ++i) values.push(i);
for (let i = 0xFFFFFFFF; i !== 0xFFFFFFFF - 0xFFFF - 1; --i) values.push(i);

for (const i of values) {
    let b = new LowLevelBinary();
    b.uint32(i);
    const v = b.getUint32(0);
    if (v !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
}
