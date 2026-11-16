// /u greedy character classes (\w, [a-z], \d) that hold no non-BMP or surrogate members, over UTF-16 text.
function splitmix32(seed) {
    var a = seed;
    return function () {
        a |= 0; a = (a + 0x9e3779b9) | 0;
        var t = a ^ (a >>> 16); t = Math.imul(t, 0x21f0aaad);
        t = t ^ (t >>> 15); t = Math.imul(t, 0x735a2d97);
        return ((t = t ^ (t >>> 15)) >>> 0) / 4294967296;
    };
}
let rnd = splitmix32(42);
let words = [];
for (let i = 0; i < 60; ++i) {
    let n = 300 + (rnd() * 400 | 0);
    let w = "";
    for (let j = 0; j < n; ++j)
        w += String.fromCharCode(0x61 + (rnd() * 26 | 0));
    words.push(w + (rnd() * 100000 | 0));
}
let text = words.join(" あ ");

let identifier = /^(?:\w+(?: あ )?)+$/u;
let word = /^(?:[a-z]+\d+(?: あ )?)+$/u;
for (let i = 0; i < 2000; ++i) {
    if (!identifier.test(text) || !word.test(text))
        throw new Error("bad match");
}
