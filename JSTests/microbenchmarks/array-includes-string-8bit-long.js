function makeString(len, ch) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += ch;
    return s;
}

function test(arr, key) {
    return arr.includes(key);
}
noInline(test);

let arr = [];
for (let i = 0; i < 64; ++i)
    arr.push(String.fromCharCode(65 + (i % 26)) + makeString(63, "x"));

let key = "@" + makeString(63, "x");
let result = 0;
for (let i = 0; i < 5e4; ++i)
    result += test(arr, key) ? 1 : 0;

if (result !== 0)
    throw new Error("bad result: " + result);
