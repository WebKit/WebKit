function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function codePointAt(string, index) { return string.codePointAt(index); }
noInline(codePointAt);

function reference(string, index) {
    let lead = string.charCodeAt(index);
    if (lead < 0xd800 || lead > 0xdbff || index + 1 >= string.length)
        return lead;
    let trail = string.charCodeAt(index + 1);
    if (trail < 0xdc00 || trail > 0xdfff)
        return lead;
    return (lead - 0xd800) * 0x400 + (trail - 0xdc00) + 0x10000;
}

let strings = [
    "abc",
    "a\u{1F600}b",
    "\u{1F600}",
    "\ud800",
    "x\ud800",
    "\ud800x",
    "\udc00\ud800",
    "あいう",
    "\u{10FFFF}\u{10000}",
];

for (let i = 0; i < testLoopCount; ++i) {
    let string = strings[i % strings.length];
    for (let j = 0; j < string.length; ++j)
        shouldBe(codePointAt(string, j), reference(string, j), `${escape(string)}.codePointAt(${j})`);
}
