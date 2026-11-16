function test(size) {
    return new Array(size);
}
noInline(test);

let length = 0;
for (let i = 0; i < 2e6; ++i)
    length += test(i & 1).length;
if (length !== 1e6)
    throw new Error("bad length " + length);
