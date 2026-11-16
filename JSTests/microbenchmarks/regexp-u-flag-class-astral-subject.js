// BMP-only /u character class scanning a subject dense in surrogate pairs.
let unit = "\u{1F600}a\u{1F601}bcd";
let text = "";
for (let i = 0; i < 4000; ++i)
    text += unit + (i % 10);

let re = /^(?:[\u{1F600}\u{1F601}]+[a-z0-9]+)+$/u;
for (let i = 0; i < 1500; ++i) {
    if (!re.test(text))
        throw new Error("bad match");
}
