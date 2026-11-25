function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

function match(regexp, string) {
    let result = regexp.exec(string);
    if (!result)
        return null;
    return [result.index, ...result];
}

for (let i = 0; i < testLoopCount; ++i) {
    let disjoint = /([ab]x)|([cd]y(z)?)|([ef]q)|(g+)h|[i-k]{2}/;
    shouldBe(match(disjoint, "ax"), [0, "ax", "ax", undefined, undefined, undefined, undefined]);
    shouldBe(match(disjoint, "aqcyz"), [2, "cyz", undefined, "cyz", "z", undefined, undefined]);
    shouldBe(match(disjoint, "cqfq"), [2, "fq", undefined, undefined, undefined, "fq", undefined]);
    shouldBe(match(disjoint, "ggghij"), [0, "gggh", undefined, undefined, undefined, undefined, "ggg"]);
    shouldBe(match(disjoint, "zzj"), null);
    shouldBe(match(disjoint, "zjk"), [1, "jk", undefined, undefined, undefined, undefined, undefined]);

    let disjointCharacters = /(ax)|(cy(z)?)|(eq)|(g+)h|i{2}/;
    shouldBe(match(disjointCharacters, "ax"), [0, "ax", "ax", undefined, undefined, undefined, undefined]);
    shouldBe(match(disjointCharacters, "aqcyz"), [2, "cyz", undefined, "cyz", "z", undefined, undefined]);
    shouldBe(match(disjointCharacters, "cqeq"), [2, "eq", undefined, undefined, undefined, "eq", undefined]);
    shouldBe(match(disjointCharacters, "ggghii"), [0, "gggh", undefined, undefined, undefined, undefined, "ggg"]);
    shouldBe(match(disjointCharacters, "zzi"), null);
    shouldBe(match(disjointCharacters, "zii"), [1, "ii", undefined, undefined, undefined, undefined, undefined]);

    let identical = /(?:[ab](c))|(?:[de]f)|(?:[ab]g)|(?:[de]h)/;
    shouldBe(match(identical, "ag"), [0, "ag", undefined]);
    shouldBe(match(identical, "ac"), [0, "ac", "c"]);
    shouldBe(match(identical, "dh"), [0, "dh", undefined]);
    shouldBe(match(identical, "dxbg"), [2, "bg", undefined]);
    shouldBe(match(identical, "abdx"), null);

    let identicalCharacters = /(?:a(c))|(?:df)|(?:ag)|(?:dh)/;
    shouldBe(match(identicalCharacters, "ag"), [0, "ag", undefined]);
    shouldBe(match(identicalCharacters, "ac"), [0, "ac", "c"]);
    shouldBe(match(identicalCharacters, "dh"), [0, "dh", undefined]);
    shouldBe(match(identicalCharacters, "dxag"), [2, "ag", undefined]);
    shouldBe(match(identicalCharacters, "aadx"), null);

    let mixedCharactersAndClasses = /ax|[a]y|[bc]z|dw|[d]v/;
    shouldBe(match(mixedCharactersAndClasses, "ay"), [0, "ay"]);
    shouldBe(match(mixedCharactersAndClasses, "dv"), [0, "dv"]);
    shouldBe(match(mixedCharactersAndClasses, "cw"), null);
    shouldBe(match(mixedCharactersAndClasses, "abcz"), [2, "cz"]);
    shouldBe(match(mixedCharactersAndClasses, "adw"), [1, "dw"]);

    let nonBMP = /([\u{1f0a1}\u{1f0b1}]{2,4})|(?:[\u{1f0ae}\u{1f0be}]{2,4})|(?:[\u{1f0a2}\u{1f0b2}]{2,4})|(?:[\u{1f0a1}\u{1f0b1}]xy)/u;
    shouldBe(match(nonBMP, "\u{1f0a1}xy"), [0, "\u{1f0a1}xy", undefined]);
    shouldBe(match(nonBMP, "\u{1f0ae}\u{1f0a1}\u{1f0b1}"), [2, "\u{1f0a1}\u{1f0b1}", "\u{1f0a1}\u{1f0b1}"]);
    shouldBe(match(nonBMP, "\u{1f0ae}\u{1f0a2}\u{1f0b2}\u{1f0a2}"), [2, "\u{1f0a2}\u{1f0b2}\u{1f0a2}", undefined]);
    shouldBe(match(nonBMP, "\ud83c\u{1f0a1}\u{1f0be}"), null);
    shouldBe(match(nonBMP, "\u{1f0a1}\u{1f0b1}"), [0, "\u{1f0a1}\u{1f0b1}", "\u{1f0a1}\u{1f0b1}"]);

    let nonBMPCharacters = /(\u{1f0a1}{2,4})|(?:\u{1f0ae}{2,4})|(?:\u{1f0a2}{2,4})|(?:\u{1f0a1}xy)/u;
    shouldBe(match(nonBMPCharacters, "\u{1f0a1}xy"), [0, "\u{1f0a1}xy", undefined]);
    shouldBe(match(nonBMPCharacters, "\u{1f0ae}\u{1f0a1}\u{1f0a1}"), [2, "\u{1f0a1}\u{1f0a1}", "\u{1f0a1}\u{1f0a1}"]);
    shouldBe(match(nonBMPCharacters, "\u{1f0ae}\u{1f0a2}\u{1f0a2}\u{1f0a2}"), [2, "\u{1f0a2}\u{1f0a2}\u{1f0a2}", undefined]);
    shouldBe(match(nonBMPCharacters, "\ud83c\u{1f0a1}\u{1f0ae}"), null);

    let nonBMPMinimumOne = /(\u{1f0a1})+a|(\u{1f0a2})+b|(\u{1f0a3})+c/u;
    shouldBe(match(nonBMPMinimumOne, "xx\ud83c"), null);
    shouldBe(match(nonBMPMinimumOne, "x\ud83c\u{1f0a2}b"), [2, "\u{1f0a2}b", undefined, "\u{1f0a2}", undefined]);
    shouldBe(match(nonBMPMinimumOne, "\u{1f0a3}\u{1f0a3}c\ud83c"), [0, "\u{1f0a3}\u{1f0a3}c", undefined, undefined, "\u{1f0a3}"]);

    let mixed = /[a\u{10000}]b|[c]+d|[e]f|[\u{10001}-\u{10003}]/u;
    shouldBe(match(mixed, "\u{10000}b"), [0, "\u{10000}b"]);
    shouldBe(match(mixed, "cccd"), [0, "cccd"]);
    shouldBe(match(mixed, "x\u{10002}g"), [1, "\u{10002}"]);
    shouldBe(match(mixed, "\u{10000}c"), null);

    let mixedCharacters = /\u{10000}|c+d|ef|\u{10002}/u;
    shouldBe(match(mixedCharacters, "x\u{10000}"), [1, "\u{10000}"]);
    shouldBe(match(mixedCharacters, "cccd"), [0, "cccd"]);
    shouldBe(match(mixedCharacters, "x\u{10002}g"), [1, "\u{10002}"]);
    shouldBe(match(mixedCharacters, "\u{10001}c"), null);

    shouldBe("ax cyz fq".match(/([ab]x)|([cd]y(z)?)|([ef]q)/g), ["ax", "cyz", "fq"]);
    shouldBe("ax cyz eq".match(/(ax)|(cy(z)?)|(eq)/g), ["ax", "cyz", "eq"]);

    // A failing alternative that skips ahead must not leave its captures behind.
    shouldBe(match(/[a](?=(b))[b][c]|[x][y][z]|[a][b][d]/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/[a](?=(b))[b][c]|[x][y][z]|[q][r][s]/, "abdxyz"), [3, "xyz", undefined]);
    shouldBe(match(/[a]([b])[c]|[x][y][z]|[a][b][d]/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/[a](?=(b))[b][c]|[x][y][z]|[a][b]\1[d]/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/[a][b](?<=(b))[c]|[x][y][z]|[a][b][d]/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/[a]([b])+[c]|[x][y]|[q][r]/, "abbqr"), [3, "qr", undefined]);

    shouldBe(match(/a(?=(b))bc|xyz|abd/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/a(?=(b))bc|xyz|qrs/, "abdxyz"), [3, "xyz", undefined]);
    shouldBe(match(/a(b)c|xyz|abd/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/a(?=(b))bc|xyz|ab\1d/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/ab(?<=(b))c|xyz|abd/, "abd"), [0, "abd", undefined]);
    shouldBe(match(/a(b)+c|xy|qr/, "abbqr"), [3, "qr", undefined]);

    let ignoreCaseModifierCharacters = /ax|bx|(?i:cx)|dx|ex/;
    shouldBe(match(ignoreCaseModifierCharacters, "CX"), [0, "CX"]);
    shouldBe(match(ignoreCaseModifierCharacters, "AXDXax"), [4, "ax"]);

    let ignoreCaseModifierClasses = /[a]x|[b]x|(?i:[c]x)|[d]x|[e]x/;
    shouldBe(match(ignoreCaseModifierClasses, "CX"), [0, "CX"]);
    shouldBe(match(ignoreCaseModifierClasses, "AXDXax"), [4, "ax"]);
}
