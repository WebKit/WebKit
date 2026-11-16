function test(str, re)
{
    return str.replace(re, ",");
}
noInline(test);

// 4-6 digit numbers: the thousands-separator idiom produces exactly one match.
const strs = [];
for (let k = 0; k < 16; k++) {
    const digits = 4 + (k % 3);
    let s = String(1 + (k % 9));
    for (let d = 1; d < digits; d++)
        s += String((k + d * 7) % 10);
    strs.push(s);
}

const re = /\B(?=(\d{3})+(?!\d))/g;

let result;
for (let i = 0; i < 2000000; ++i)
    result = test(strs[i & 15], re);

if (test("1234", re) !== "1,234")
    throw new Error("bad result: " + test("1234", re));
if (test("123456", re) !== "123,456")
    throw new Error("bad result: " + test("123456", re));
