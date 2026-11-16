function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

// Supplementary code points whose low 16 bits spell a syntax character, a
// punctuator, a control character or whitespace must pass through unchanged.
shouldBe(RegExp.escape("\u{2002A}"), "\u{2002A}"); // low 16 bits: '*'
shouldBe(RegExp.escape("\u{20009}"), "\u{20009}"); // low 16 bits: '\t'
shouldBe(RegExp.escape("\u{2002C}"), "\u{2002C}"); // low 16 bits: ','
shouldBe(RegExp.escape("\u{20020}"), "\u{20020}"); // low 16 bits: ' '
shouldBe(RegExp.escape("\u{12000}"), "\u{12000}"); // low 16 bits: U+2000 (EN QUAD)
shouldBe(RegExp.escape("\u{1D800}"), "\u{1D800}"); // low 16 bits: lead surrogate
shouldBe(RegExp.escape("\u{1FEFF}"), "\u{1FEFF}"); // low 16 bits: U+FEFF (BOM)
for (const s of ["\u{2002A}", "\u{20009}", "\u{2002C}", "\u{20020}", "\u{12000}", "\u{1D800}", "\u{1FEFF}", "\u{1F600}"]) {
    for (const flags of ["", "u", "v"])
        shouldBe(new RegExp(RegExp.escape(s), flags).test(s), true);
}

// Every BMP code unit RegExp.escape rewrites, combined with every plane.
const escapedCodeUnits = [..."^$\\.*+?()[]{}|/,-=<>#&!%:;@~'`\"\t\n\v\f\r       　﻿"].map((c) => c.charCodeAt(0));
for (let codeUnit = 0x2000; codeUnit <= 0x200a; codeUnit++)
    escapedCodeUnits.push(codeUnit);
escapedCodeUnits.push(0xd800, 0xd83d, 0xdbff, 0xdc00, 0xde00, 0xdfff);
for (const codeUnit of escapedCodeUnits)
    shouldBe(RegExp.escape("_" + String.fromCharCode(codeUnit)) !== "_" + String.fromCharCode(codeUnit), true);
for (let plane = 1; plane <= 16; plane++) {
    for (const codeUnit of escapedCodeUnits) {
        const s = String.fromCodePoint((plane << 16) | codeUnit);
        shouldBe(RegExp.escape(s), s);
    }
}
