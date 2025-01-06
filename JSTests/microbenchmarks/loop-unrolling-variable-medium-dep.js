//@skip if $memoryLimited
let arr = Array(1000).fill(0)

for (let i = 0; i < arr.length; ++i) {
    arr[i] = i;
}

function main(b, d) {
    "use strict";

    b = b | 0
    d = d | 0

    if (arr.length != 1000 || b < 0 || d < 0)
        return;

    let sum = 0

    for (let i = 1; i < d; ++i) {
        arr[i] = ((arr[i - 1] | 0) + 1) | 0
        sum += arr[i]
    }
    return sum
}
noInline(main)

const expected = 499500

for (let i = 0; i < 1e4; ++i) {
    let r = main(5, 1000)
    if (r != expected)
        throw "Error: expected " + expected + " got " + r
}
