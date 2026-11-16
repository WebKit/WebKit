let text = "abababab-abab-ababab-ab-abababab-".repeat(300);
let re = /(?<=(?:ab){3})-/g;
let result = 0;
for (let i = 0; i < 1e4; ++i) {
    re.lastIndex = 0;
    while (re.exec(text))
        result++;
}
if (result !== 3 * 300 * 1e4)
    throw new Error("Bad result: " + result);
