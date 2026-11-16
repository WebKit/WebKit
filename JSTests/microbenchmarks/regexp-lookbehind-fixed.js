let text = "The quick brown fox jumps over the lazy dog. ".repeat(80) + "price: $42";
let re = /(?<=\$)\d+/;
let result = 0;
for (let i = 0; i < 1e4; ++i)
    result += re.exec(text).index;
if (result !== 3608 * 1e4)
    throw new Error("Bad result: " + result);
