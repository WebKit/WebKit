let text = "The quick brown fox jumps over the lazy dog. ".repeat(80) + "\u{1F600}42";
let re = /(?<=[\u{1F600}-\u{1F64F}])\d+/u;
let result = 0;
for (let i = 0; i < 1e4; ++i)
    result += re.exec(text).index;
if (result !== 3602 * 1e4)
    throw new Error("Bad result: " + result);
