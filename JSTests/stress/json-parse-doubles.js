function shouldBe(actual, expected, text) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value for ${text}: ${actual}, expected: ${expected}`);
}

let texts = [
    "0.1", "0.2", "0.3", "-0.0", "-0e5", "0e999999", "1e0", "1E-0", "1e+0", "1e23", "1e22", "1e-22", "1e-23",
    "9007199254740992", "9007199254740993", "9007199254740993.0", "9007199254740992.5", "18014398509481985",
    "1.7976931348623157e308", "1.7976931348623159e308", "2.2250738585072011e-308", "2.2250738585072012e-308",
    "4.9e-324", "5e-324", "2.4703282292062327e-324", "2.4703282292062328e-324", "1e-400", "1e400", "-1e400",
    "0.9999999999999999", "0.99999999999999999", "3.0000000000000004", "1.0000000000000002",
    "1.00000000000000011102230246251565404236316680908203125", "123456789012345678901234567890",
    "9999999999999999999", "99999999999999999999", "10000000000000000000", "12345678901", "-123456789012",
    "0.12345678", "0.123456789", "0.1234567890123456", "0.12345678901234567", "12345678.12345678",
    "0.0000000000000000000000001", "0.000000012345678901234567", "1.12345678e5", "0.99999999e-5",
    "0.0012345678901234567", "0.0000000000000000000012345678901234567", "0.00000000000000000000123456789012345678901",
    "0.1234567890123456789", "0.12345678901234567890", "0.000000000000000000000", "1700000000000", "1234567890123456789",
    "1e" + "0".repeat(50) + "5", "1e-0000000000000000000001", "1e99999", "1e100000", "1e-99999", "1e-100000",
];
// Leading zeros of the fraction that offset an exponent too large to be read exactly.
for (let [zeros, exponent] of [[99998, 99999], [100004, 100005], [999999, 1000000]]) {
    let text = "0." + "0".repeat(zeros) + "1e" + exponent;
    texts.push(text, "-" + text, "0." + "0".repeat(zeros) + "12345678901234567e" + (exponent + 1));
}
let seed = 7;
function rand() { seed = (seed * 16807) % 2147483647; return seed / 2147483647; }
for (let i = 0; i < 400; ++i) {
    let value = (rand() - 0.5) * Math.pow(10, (i % 60) - 30);
    texts.push(String(value), value.toPrecision(1 + (i % 21)), value.toExponential(i % 21), value.toFixed(i % 21));
}
for (let digits = 1; digits <= 22; ++digits) {
    let s = "";
    for (let j = 0; j < digits; ++j)
        s += String((j * 7 + digits) % 10);
    s = s.replace(/^0+(?=\d)/, "");
    for (let dot = 1; dot < s.length; ++dot)
        texts.push(s.slice(0, dot) + "." + s.slice(dot));
}

// Objects of one shape, parsed more than once, have their keys found in JSONCache, and the values after them are
// read from its cursor. JSONCache matches a key only when at least 32 characters of source remain.
const padding = " ".repeat(40);
function parseObjects(prefix) {
    let text = `[${prefix}${texts.map(text => `{"a":${text},"b":${text}}`).join(",")}]${padding}`;
    let result;
    for (let i = 0; i < 3; ++i)
        result = JSON.parse(text);
    return result.slice(prefix ? 1 : 0);
}

let array = JSON.parse(`[${texts.join(",")}]`);
let values = Object.values(JSON.parse(`{${texts.map((text, i) => `"k${i}":${text}`).join(",")}}`));
let wide = JSON.parse(`["\u3042",${texts.join(",")}]`);
let objects = parseObjects("");
let wideObjects = parseObjects(`"\u3042",`);
for (let i = 0; i < texts.length; ++i) {
    let expected = Number(texts[i]);
    shouldBe(JSON.parse(texts[i]), expected, texts[i]);
    shouldBe(array[i], expected, texts[i]);
    shouldBe(values[i], expected, texts[i]);
    shouldBe(wide[i + 1], expected, texts[i]);
    shouldBe(objects[i].a, expected, texts[i]);
    shouldBe(objects[i].b, expected, texts[i]);
    shouldBe(wideObjects[i].a, expected, texts[i]);
    shouldBe(wideObjects[i].b, expected, texts[i]);
}

// Characters above Latin-1 whose low byte is a digit must still end the fraction, at every position in the word.
for (let length = 1; length <= 17; ++length) {
    let fraction = "1234567890123456789".slice(0, length);
    for (let character of ["\u0130", "\u0139", "\u3030", "\uff10"]) {
        for (let text of [`[0.${fraction}${character}]`, `[0.${fraction}${character}12345678]`, `{"a":1.${fraction}${character}}`]) {
            let error = null;
            try {
                JSON.parse(text);
            } catch (e) {
                error = e;
            }
            if (!(error instanceof SyntaxError))
                throw new Error(`did not throw SyntaxError: ${escape(text)}`);
        }
    }
}
