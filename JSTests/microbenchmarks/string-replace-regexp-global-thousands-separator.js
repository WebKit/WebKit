function test(str, re)
{
    return str.replace(re, ",");
}
noInline(test);

// Mixed 4-8 digit numbers, as produced by the common numberWithCommas idiom.
const strs = [];
for (let k = 0; k < 16; k++)
    strs.push(String(Math.round(1000 * Math.pow(1.9, k))));

const re = /\B(?=(\d{3})+(?!\d))/g;

let result;
for (let i = 0; i < 2000000; ++i)
    result = test(strs[i & 15], re);

if (test("1234567", re) !== "1,234,567")
    throw new Error("bad result: " + test("1234567", re));
