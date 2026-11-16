function test(start, count) {
    let n = start;
    for (let i = 0; i < count; i++)
        n--;
    return n;
}
noInline(test);

const start = 0x123456789abcdef0123456789abcdefn;
let result = 0n;
for (let i = 0; i < 2000; i++)
    result = test(start, 2000);
if (result !== start - 2000n)
    throw new Error("bad result: " + result);
