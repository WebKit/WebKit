function test(collator, a, b, n) {
    let result = 0;
    for (let i = 0; i < n; ++i)
        result += collator.compare(a, b);
    return result;
}

let collator = new Intl.Collator("en");
let suffix = "he quick brown fox jumps over the lazy dog 0123456789 ABCDEFGHIJ";
let a = ("a" + suffix).split("").join("");
let b = ("b" + suffix).split("").join("");

let result = test(collator, a, b, 2e6);
if (result !== -2e6)
    throw new Error("Bad result: " + result);
