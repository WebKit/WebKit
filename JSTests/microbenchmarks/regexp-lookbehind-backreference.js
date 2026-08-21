let text = "abab-abcabc-xyxy-abcabd-aa-a-".repeat(40);
let re = /(?<=\1(\w+))-/g;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 160 * 1e4)
    throw new Error("Bad result: " + result);
