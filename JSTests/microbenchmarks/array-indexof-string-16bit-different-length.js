// Array of 16-bit strings whose lengths all differ from the search element.
// FTL fast loop should be able to reject each element on the length check
// alone without bailing to the C++ slow path just because the element is 16-bit.

function test(arr, key) {
    return arr.indexOf(key);
}
noInline(test);

let arr = [];
for (let i = 0; i < 1000; ++i) {
    // 16-bit string, length 5..14 (never 4)
    arr.push("あ".repeat(5 + (i % 10)));
}

let key = "abcd"; // 8-bit, length 4
let result = 0;
for (let i = 0; i < 5e5; ++i)
    result += test(arr, key);

if (result !== -5e5)
    throw new Error("bad result: " + result);
