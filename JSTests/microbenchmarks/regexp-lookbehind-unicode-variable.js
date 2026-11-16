let text = "\u{1F600} value1; \u{1F600}\u{1F601} value22; \u{1F600}\u{1F601}\u{1F602} value333; ".repeat(40);
let re = /(?<=[\u{1F600}-\u{1F64F}]+ )\w+/gu;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 120 * 1e4)
    throw new Error("Bad result: " + result);
