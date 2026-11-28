function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function test(regExp, string, expected) {
    let match = regExp.exec(string);
    shouldBe(match && `${match.index}:${match.join()}`, expected);
}

let long = "x".repeat(300) + ".mjs";

for (let i = 0; i < testLoopCount; i++) {
    test(/\.(?:js|ts|mjs)$/, "src/app.mjs", "7:.mjs");
    test(/\.(?:js|ts|mjs)$/, "src/app.ts", "7:.ts");
    test(/\.(?:js|ts|mjs)$/, "src/app.mjsx", null);
    test(/\.(?:js|ts|mjs)$/, "js", null);
    test(/\.(js|ts|mjs)$/, "src/app.mjs", "7:.mjs,mjs");
    test(/(?<unit>px|em|rem|%)$/, "1.5rem", "3:rem,rem");
    test(/\.(?:j(?:pe?g|fif)|png)$/, "a.jpeg", "1:.jpeg");
    test(/\.(?:jpe?g|png)$/i, "A.JPEG", "1:.JPEG");
    test(/\.m?js$/, "a.mjs", "1:.mjs");
    test(/\.m?js$/, "a.js", "1:.js");
    test(/\.[cm]?[jt]sx?$/, "a.b.ctsx", "3:.ctsx");
    test(/a{1,3}$/, "baaaa", "2:aaa");
    test(/a{1,3}?$/, "baaaa", "2:aaa");
    test(/a{2,5}$/, "ba", null);
    test(/a{2,5}$/, "aaa", "0:aaa");
    test(/\.\d{1,3}$/, "10.0.0.255", "6:.255");
    test(/(?:ab){1,3}$/, "abababab", "2:ababab");
    test(/(ab|c){2}$/, "abcab", "2:cab,ab");
    test(/(?:ab|c){2}?d$/, "abccd", "2:ccd");
    test(/(?:\.min)?\.js$/, "a.min.js", "1:.min.js");
    test(/(?:\.min)?\.js$/, "a.max.js", "5:.js");
    test(/\/?$/, "a/b/", "3:/");
    test(/\/?$/, "a/b", "3:");
    test(/(?:|a|ab)$/, "cab", "1:ab");
    test(/(?<=\/)[a-z]{1,3}$/, "a/bcd", "2:bcd");
    test(/(?<=\/)[a-z]{1,3}$/, "a/bcde", null);
    test(/(?<!\.)(?:js|mjs)$/, "a.mjs", "3:js");
    test(/(?=..$)(?:a|bc)?c$/, "abc", null);
    test(/\b(?:px|rem)$/, "1 rem", "2:rem");
    test(/\.(?:js|mjs)$|x/, "xyz.css", "0:x");
    test(/\.(?:js|mjs)$|^x/, "xyz.mjs", "0:x");
    test(/\.m?js$/m, "a.mjs\nb.css", "1:.mjs");
    test(/\.\w+$/, "a.b.cd", "3:.cd");
    test(/(?:ab)*$/, "cabab", "1:abab");
    test(/(a|bb)\1?$/, "xbbbb", "1:bbbb,bb");

    test(/[\u{1F600}a]{1,2}$/u, "x\u{1F600}\u{1F600}", "1:\u{1F600}\u{1F600}");
    test(/.{2}$/u, "x\u{1F600}\u{1F600}", "1:\u{1F600}\u{1F600}");
    test(/[^a]?b$/u, "\u{1F600}b", "0:\u{1F600}b");
    test(/\u{1F600}?a$/u, "\u{1F600}a", "0:\u{1F600}a");
    test(/(?:a|bc)$/u, "\u{1F600}a", "2:a");
    test(/\uDE00?a$/u, "\u{1F600}a", "2:a");
    test(/[\uDE00x]{0,2}$/u, "\u{1F600}", "2:");
    test(/\uDE00?a$/, "\u{1F600}a", "1:\uDE00a");
    test(/.{1,2}$/, "\u{1F600}a", "1:\uDE00a");
    test(/[\q{abc|d}]$/v, "xabc", "1:abc");
    test(/[\q{\u{1F600}a|b}x]?$/v, "\u{1F600}a", "0:\u{1F600}a");
    test(/\.(?:js|mjs)$/, "\u65E5\u672C\u8A9E/\u65E5\u672C\u8A9E.mjs", "7:.mjs");

    shouldBe(/\.(?:js|ts|mjs)$/.test(long), true);
    shouldBe(/\.(?:ts|tsx)$/.test(long), false);
    shouldBe(long.search(/\.(?:js|mjs)$/), 300);
    shouldBe(long.split(/s?$/).length, 2);
    shouldBe("a.js".replace(/(?:js|mjs)?$/g, "<$&>"), "a.<js><>");
    shouldBe(/\.(m?)js$/d.exec("a.mjs").indices.join(), "1,5,2,3");

    let regExp = /\.m?js$/g;
    regExp.lastIndex = 1;
    test(regExp, "a.mjs", "1:.mjs");
    shouldBe(regExp.lastIndex, 5);
    regExp.lastIndex = 2;
    test(regExp, "a.mjs", null);
    shouldBe(regExp.lastIndex, 0);

    regExp = /m?js$/y;
    test(regExp, "a.mjs", null);
    regExp.lastIndex = 3;
    test(regExp, "a.mjs", "3:js");
}
