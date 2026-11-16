function test(x) {
    return (x & 1) ? [x] : [];
}
noInline(test);

let length = 0;
for (let i = 0; i < 2e6; ++i)
    length += test(i).length;
if (length !== 1e6)
    throw new Error("bad length " + length);
