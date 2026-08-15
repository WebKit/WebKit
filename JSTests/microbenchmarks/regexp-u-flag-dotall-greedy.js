let texts = ["あ" + "abcdefghij".repeat(100) + "END", "い" + "abcdefghij".repeat(100) + "END"];
let re1 = /a.*END/su;
let re2 = /a.*?END/su;
let re3 = /[^]{500}j/u;
let result = 0;
for (let i = 0; i < 2e4; ++i) {
    let text = texts[i & 1];
    if (re1.test(text))
        result++;
    if (re2.test(text))
        result++;
    if (re3.test(text))
        result++;
}
if (result !== 3 * 2e4)
    throw new Error("Bad result: " + result);
