let text = "doable unable readable unreadable stable unstable notable untenable capable ".repeat(40);
let re = /(?<=\b(?!un)\w+)able/g;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 200 * 1e4)
    throw new Error("Bad result: " + result);
