let text = "1234567890".repeat(30);
let re = /(?<=\d)(?=(?:\d{3})+$)/g;
let result = 0;
for (let i = 0; i < 2e3; ++i)
    result += text.replace(re, ",").length;
if (result !== 399 * 2e3)
    throw new Error("Bad result: " + result);
