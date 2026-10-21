function test(a, b) {
    return a.buffer === b.buffer;
}
noInline(test);

const buffer = new ArrayBuffer(64);
const views = [new Uint8Array(buffer, 0, 16), new Uint8Array(buffer, 16, 16), new Uint8Array(16)];

let count = 0;
for (let i = 0; i < 1e7; i++) {
    if (test(views[0], views[i % 3]))
        count++;
}

if (count !== 6666667)
    throw new Error("bad: " + count);
