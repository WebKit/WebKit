function test(arr, key) {
    return arr.indexOf(key);
}
noInline(test);

let arr = [];
for (let i = 0; i < 1000; ++i)
    arr.push("a".repeat(5 + (i % 10)));

let key = "abcd";
let result = 0;
for (let i = 0; i < 5e5; ++i)
    result += test(arr, key);

if (result !== -5e5)
    throw new Error("bad result: " + result);
