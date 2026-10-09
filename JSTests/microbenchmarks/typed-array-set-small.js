function test(target, source, offset) {
    target.set(source, offset);
}
noInline(test);

const target = new Uint8Array(64);
const source = new Uint8Array(16);
for (let i = 0; i < source.length; i++)
    source[i] = i + 1;

for (let i = 0; i < 1e7; i++)
    test(target, source, i & 31);

if (target[46] !== 16)
    throw new Error("bad: " + target[46]);
