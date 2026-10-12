function test(array) {
    return array.map((x) => x + 0.5);
}
noInline(test);

let arrays = [[], [1.5]];
let length = 0;
for (let i = 0; i < 2e6; ++i)
    length += test(arrays[i & 1]).length;
if (length !== 1e6)
    throw new Error("bad length " + length);
