function shouldBe(actual, expected, text) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value for ${escape(text)}: ${actual}, expected: ${expected}`);
}

function shouldThrowSyntaxError(text) {
    let error = null;
    try {
        eval(text);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`did not throw SyntaxError: ${escape(text)}`);
}

let texts = [
    "0.1", "0.2", "0.3", "0.0", "0e5", "0E5", "0e+5", "0e-5", "0e999999", "1e0", "1E-0", "1e+0", "1e23", "1e22", "1e-22", "1e-23",
    "9007199254740992", "9007199254740993", "9007199254740993.0", "9007199254740992.5", "18014398509481985",
    "1.7976931348623157e308", "1.7976931348623159e308", "2.2250738585072011e-308", "2.2250738585072012e-308",
    "4.9e-324", "5e-324", "2.4703282292062327e-324", "2.4703282292062328e-324", "1e-400", "1e400",
    "0.9999999999999999", "0.99999999999999999", "3.0000000000000004", "1.0000000000000002",
    "1.00000000000000011102230246251565404236316680908203125", "123456789012345678901234567890",
    "9999999999999999999", "99999999999999999999", "10000000000000000000", "12345678901", "123456789012",
    "123456789012345", "1234567890123456", "12345678901234567", "123456789012345678",
    "0.12345678", "0.123456789", "0.1234567890123456", "0.12345678901234567", "12345678.12345678",
    "0.0000000000000000000000001", "0.000000012345678901234567", "1.12345678e5", "0.99999999e-5",
    "0.0012345678901234567", "0.0000000000000000000012345678901234567", "0.00000000000000000000123456789012345678901",
    "0.1234567890123456789", "0.12345678901234567890", "0.000000000000000000000", "1700000000000", "1234567890123456789",
    "1e" + "0".repeat(50) + "5", "1e-0000000000000000000001", "1e99999", "1e100000", "1e-99999", "1e-100000",
    // Forms JSON does not accept.
    "1.", "0.", "123.", "1.e5", "1.E-5", "0.e5", "12345678901234567890.", "9007199254740993.",
    ".5", ".0", ".1", ".123456789", ".1234567890123456789", ".12345678901234567890", ".5e3", ".5E-3", ".0e999999",
    ".00000000000000000000000000001", ".9999999999999999999999",
];
let seed = 7;
function rand() { seed = (seed * 16807) % 2147483647; return seed / 2147483647; }
for (let i = 0; i < 200; ++i) {
    let value = rand() * Math.pow(10, (i % 60) - 30);
    texts.push(String(value), value.toPrecision(1 + (i % 21)), value.toExponential(i % 21), value.toFixed(i % 21));
}
for (let digits = 1; digits <= 22; ++digits) {
    let s = "";
    for (let j = 0; j < digits; ++j)
        s += String((j * 7 + digits) % 10);
    s = s.replace(/^0+(?=\d)/, "");
    texts.push(s);
    for (let dot = 1; dot < s.length; ++dot)
        texts.push(s.slice(0, dot) + "." + s.slice(dot));
}
// Leading zeros other than "0." and "0e" are legacy octal or non-octal decimal literals, which are not valid in
// strict code and are covered elsewhere.
texts = texts.filter(text => !/^0\d/.test(text));

for (let text of texts) {
    let expected = Number(text);
    shouldBe(eval(text), expected, text);
    shouldBe(eval(`(${text})`), expected, text);
    shouldBe(eval(`"\u3042"; ${text}`), expected, text);
    shouldBe(eval(`[${text}, ${text}][1]`), expected, text);
    shouldBe(eval(`[${text}\u2028][0]`), expected, text);
    shouldBe(eval(`-${text}`), -expected, text);
    shouldBe(new Function(`return ${text}`)(), expected, text);
    shouldBe(new Function(`return ${text};`)(), expected, text);
    shouldBe(new Function(`"\u3042"; return ${text};`)(), expected, text);
}

// Long runs of leading fraction zeros, cancelled by a large exponent. Few forms each, since every one is over 100KB of source.
for (let [zeros, exponent] of [[99998, 99999], [100004, 100005]]) {
    let padding = "0".repeat(zeros);
    for (let text of [`0.${padding}1e${exponent}`, `.${padding}1e${exponent}`, `0.${padding}12345678901234567e${exponent + 1}`]) {
        let expected = Number(text);
        shouldBe(eval(text), expected, text);
        shouldBe(eval(`"\u3042"; ${text}`), expected, text);
        shouldBe(eval(`[${text},0]`)[0], expected, text);
        shouldBe(eval(`"\u3042"; [${text},0]`)[0], expected, text);
    }
}

// Numeric separators are not part of the scan, and must still be accepted where they are valid.
for (let [text, expected] of [
    ["1_0.5", 10.5], ["1.0_5", 1.05], ["1.5e1_0", 1.5e10], ["1_000_000.000_001", 1000000.000001], ["0.0_1", 0.01],
    [".0_1", 0.01], ["1e1_0", 1e10], ["1_2345678901234567890", 12345678901234567890], ["0.123_456_789_012_345_678_9", 0.1234567890123456789],
]) {
    shouldBe(eval(text), expected, text);
    shouldBe(eval(`"\u3042"; ${text}`), expected, text);
}

// BigInt literals have no fraction or exponent.
shouldBe(eval("12345678901234567890n"), 12345678901234567890n, "12345678901234567890n");
shouldBe(eval("1234567890n"), 1234567890n, "1234567890n");
shouldBe(eval("0n"), 0n, "0n");

// A '.' after a number that ends in '.' or an exponent starts a member access.
shouldBe(eval("1..toString()"), "1", "1..toString()");
shouldBe(eval("1.5.toString()"), "1.5", "1.5.toString()");
shouldBe(eval("1e3.toString()"), "1000", "1e3.toString()");
shouldBe(eval(".5.toString()"), "0.5", ".5.toString()");
shouldBe(eval("0..toString()"), "0", "0..toString()");
shouldBe(eval("[1.5,.5,0.5,1e3,1.][4]"), 1, "[1.5,.5,0.5,1e3,1.][4]");
shouldBe(eval("1.5?.5:1"), 0.5, "1.5?.5:1");

// Legacy octal-like literals are left to the general path.
shouldBe(eval("08.5"), 8.5, "08.5");
shouldBe(eval("09e1"), 90, "09e1");
shouldBe(eval("019.5"), 19.5, "019.5");

// Strict code rejects legacy octal and leading-zero decimal literals, including inside array literals.
for (let text of [
    "00", "01", "07", "08", "09", "010", "0012345678901234567890", "00.5", "01.5", "07.5", "08.5", "09.5", "019.5",
    "08e1", "09e1", "08.5e1", "0123456789012345678901.5", "08_0", "08.5_1", "0_1", "0_1.5",
    "[1.5,00,2]", "[1.5,07,2]", "[1.5,08.5,2]", "[1.5,09e1,2]", "[1.5,019.5,2]", "[08.5,1.5]",
]) {
    shouldThrowSyntaxError(`"use strict"; ${text}`);
    shouldThrowSyntaxError(`"use strict"; "\u3042"; ${text}`);
    shouldThrowSyntaxError(`(function () { "use strict"; return ${text}; })`);
    shouldThrowSyntaxError(`class C { m() { return ${text}; } }`);
}
for (let [text, expected] of [
    ["0", 0], ["0.5", 0.5], ["0.", 0], ["0e5", 0], ["0.0e5", 0], [".5", 0.5], ["0.0123456789012345678901", 0.0123456789012345678901],
    ["[0,0.5,0.,0e5,.5,2][4]", 0.5], ["[1.5,0.5,2][1]", 0.5],
]) {
    shouldBe(eval(`"use strict"; ${text}`), expected, text);
    shouldBe(eval(`"use strict"; "\u3042"; ${text}`), expected, text);
}

for (let text of [
    "1.5a", "1.5_", "1._5", "1_.5", "1.5e", "1.5e+", "1.5e-", "1.5e_1", "1.5e1_", "1.5e1a", ".5a", ".5_", "._5", "0.5a",
    "0.5_", "0e", "0e+", "1e", "1ea", "1.5n", "1e5n", ".5n", "0.5n", "1.5\\u0061", "1.5\u3042", "1.5\u0130", "1.5\u0661",
    "0.123456789\u0130", "0.1234567890123456789\u3030", "1.12345678\uff10", "1.5e5\u0130", "1.5$", "1.5e1$", "1.5ee1",
    "1.5e1e1", "1.5e1.5e1",
])
    shouldThrowSyntaxError(text);

// Characters above Latin-1 whose low byte is a digit must still end the fraction, at every position in the word.
for (let length = 1; length <= 17; ++length) {
    let fraction = "1234567890123456789".slice(0, length);
    for (let character of ["\u0130", "\u0139", "\u3030", "\uff10"]) {
        for (let text of [`0.${fraction}${character}`, `0.${fraction}${character}12345678`, `.${fraction}${character}`, `1.${fraction}${character}`])
            shouldThrowSyntaxError(text);
    }
    // Whitespace and line terminators above Latin-1 end the literal.
    for (let character of ["\u2028", "\u2029", "\u3000", "\ufeff", "\u00a0"]) {
        shouldBe(eval(`0.${fraction}${character}`), Number(`0.${fraction}`), `0.${fraction}`);
        shouldBe(eval(`.${fraction}${character}`), Number(`.${fraction}`), `.${fraction}`);
    }
}

// Elements of an array literal directly followed by ',' are read by the parser's fast path.
{
    let compact = eval(`[${texts.join(",")},0]`);
    let wide = eval(`"\u3042"; [${texts.join(",")},0]`);
    let functionResult = new Function(`return [${texts.join(",")},0]`)();
    for (let i = 0; i < texts.length; ++i) {
        let expected = Number(texts[i]);
        shouldBe(compact[i], expected, texts[i]);
        shouldBe(wide[i], expected, texts[i]);
        shouldBe(functionResult[i], expected, texts[i]);
    }
    shouldBe(compact.length, texts.length + 1, "length");
}
for (let [text, expected] of [
    ["[1.,2]", [1, 2]], ["[.5,0.5,1e3,1.5e-3,null,7]", [0.5, 0.5, 1000, 0.0015, null, 7]], ["[1_0.5,2]", [10.5, 2]],
    ["[1.5,...[2.5],3]", [1.5, 2.5, 3]], ["[0.5,,1.5]", [0.5, , 1.5]], ["[1.5,.5.toString(),2]", [1.5, "0.5", 2]],
    ["[1.5,1e3.toFixed(1),2]", [1.5, "1000.0", 2]], ["[1.5,1.5?.5:1,2]", [1.5, 0.5, 2]], ["[1.5,12345678901234567890,2]", [1.5, 12345678901234567890, 2]],
    ["[1.5,123456789012345678901234567890.5,2]", [1.5, 123456789012345678901234567890.5, 2]], ["[1.5,08.5,2]", [1.5, 8.5, 2]],
    ["[1.5,1.5+1,2]", [1.5, 2.5, 2]], ["[1.5,.5*2,2]", [1.5, 1, 2]],
]) {
    let actual = eval(text);
    shouldBe(actual.length, expected.length, text);
    for (let i = 0; i < expected.length; ++i) {
        shouldBe(i in actual, i in expected, text);
        shouldBe(actual[i], expected[i], text);
    }
}
shouldBe(eval("[1.5,12345678901234567890n,2]")[1], 12345678901234567890n, "BigInt element");
for (let text of ["[1.5,1.5a,2]", "[1.5,1.5_,2]", "[1.5,.5n,2]", "[1.5,1e,2]", "[1.5,1.5.5,2]", "[1.5,1.5\\u0061,2]", "[1.5,0.5\u0130,2]", "[1.5,1.5e5$,2]"])
    shouldThrowSyntaxError(text);

// Locations recorded for elements read by the fast path must match the ones the tokens would have had.
function errorColumn(text) {
    try {
        eval(text);
    } catch (e) {
        return e.column - text.indexOf("undefined.x");
    }
    throw new Error(`did not throw: ${text}`);
}
let baseline = errorColumn("[1,2,null,undefined.x]");
for (let text of ["[1.5,2.5,0.5,undefined.x]", "[.5,1e3,1.,undefined.x]", "[0.123456789012345678,12345678901234567,undefined.x]"])
    shouldBe(errorColumn(text), baseline, text);
