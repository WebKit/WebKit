import LowLevelBinary from '../LowLevelBinary.js';

let b = new LowLevelBinary();

for (let i = 0; i !== 1024 * 3; ++i)
    b.uint32(i);
for (let i = 0; i !== 1024 * 3; ++i) {
    let v = b.getUint32(i * 4);
    if (v !== i)
        throw new Error(`Wrote "${i}" and read back "${v}"`);
}
