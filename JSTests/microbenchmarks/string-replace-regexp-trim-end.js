function test(str, re)
{
    return str.replace(re, "");
}
noInline(test);

const strs = [];
for (let k = 0; k < 16; k++)
    strs.push("a".repeat(200 + k * 50) + " \t\n".repeat(k & 3));

const re = /\s+$/;

let result;
for (let i = 0; i < 1000000; ++i)
    result = test(strs[i & 15], re);

if (test("hello   ", re) !== "hello")
    throw new Error("bad result: " + test("hello   ", re));
