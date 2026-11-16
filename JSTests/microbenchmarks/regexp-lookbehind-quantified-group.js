let text = "1,234,567 12,345 999 1,000,000 42 7,777,777,777 ".repeat(200);
let re = /(?<=\b(?:\d{1,3},)+\d{3}) /g;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 4 * 200 * 1e4)
    throw new Error("Bad result: " + result);
