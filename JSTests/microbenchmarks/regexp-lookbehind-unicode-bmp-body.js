let text = "key1 = value1; key22 = value22; key333 = value333; ".repeat(40) + "\u{1F600}";
let re = /(?<=key\d+ = )\w+/gu;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 120 * 1e4)
    throw new Error("Bad result: " + result);
