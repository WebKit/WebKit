const size = 256;
const table = new Array(size);
for (let i = 0; i < size; ++i)
    table[i] = (i & 1) ? 0x80000000 + i * 0x10001 : i * 0x1001;

const indices = new Int32Array(size);
let state = 0x12345678;
for (let i = 0; i < size; ++i) {
    state = (Math.imul(state, 1103515245) + 12345) | 0;
    indices[i] = (state >>> 8) & (size - 1);
}

function run()
{
    let sum = 0;
    for (let i = 0; i < size; ++i)
        sum ^= table[indices[i]];
    return sum;
}
noInline(run);

let result = 0;
for (let i = 0; i < 4e5; ++i)
    result ^= run();
