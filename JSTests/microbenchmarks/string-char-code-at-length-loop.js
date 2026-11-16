function count(string) {
    let result = 0;
    for (let i = 0; i < string.length; ++i) {
        let code = string.charCodeAt(i);
        if (code >= 0xD800 && code <= 0xDBFF)
            continue;
        ++result;
    }
    return result;
}
noInline(count);

let latin1 = "The quick brown fox jumps over the lazy dog. ".repeat(20);
let utf16 = "いろはにほへと\u{1F600}".repeat(100);

for (let i = 0; i < 2e5; ++i) {
    if (count(latin1) !== 900)
        throw new Error("bad result");
    if (count(utf16) !== 800)
        throw new Error("bad result");
}
