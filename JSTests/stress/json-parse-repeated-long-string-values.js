function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected: ${JSON.stringify(expected)}`);
}

const prefix = "https://example.com/a/very/long/common/prefix/shared/by/all/";
const values = [
    prefix + "one.js",
    prefix + "two.js",
    prefix + "one.jsx",
    prefix + "one.j",
    prefix + "onf.js",
    prefix + "one.js" + "x".repeat(150),
    "x".repeat(31),
    "y".repeat(256),
    "z".repeat(257),
    prefix + "\u00e9t\u00e9.js",
    prefix + "escaped\\\"quote.js",
    prefix + "tab\there.js",
];

for (let iteration = 0; iteration < 20; ++iteration) {
    const objects = [];
    for (let i = 0; i < 200; ++i)
        objects.push({ url: values[(i * 7 + iteration) % values.length], line: i });
    const text = JSON.stringify(objects);
    const parsed = JSON.parse(text);
    shouldBe(parsed.length, objects.length);
    for (let i = 0; i < objects.length; ++i) {
        shouldBe(parsed[i].url, objects[i].url);
        shouldBe(parsed[i].line, i);
    }
    shouldBe(JSON.stringify(parsed), text);
    if (iteration % 5 === 4)
        gc();
}

// A cached value followed by something other than its closing quote must not match.
for (let i = 0; i < 10; ++i)
    JSON.parse(`{"url":"${values[0]}","x":1}`);
shouldBe(JSON.parse(`{"url":"${values[0]}x","x":1}`).url, values[0] + "x");
shouldBe(JSON.parse(`{"url":"${values[0]}\\n","x":1}`).url, values[0] + "\n");
shouldBe(JSON.parse(`{"url":"${values[0]}"}`).url, values[0]);

let threw = false;
try {
    JSON.parse(`{"url":"${values[0]}`);
} catch (e) {
    threw = e instanceof SyntaxError;
}
shouldBe(threw, true);

// Values that differ only in their last character, around the lengths where the comparison changes strides.
for (const length of [31, 32, 33, 47, 48, 49, 63, 64, 65, 95, 96, 97, 255, 256]) {
    const base = prefix.repeat(5).slice(0, length - 1);
    const a = base + "a";
    const b = base + "b";
    for (let i = 0; i < 20; ++i) {
        const value = i % 3 ? a : b;
        shouldBe(JSON.parse(`{"url":"${value}","x":${i}}`).url, value);
    }
    shouldBe(JSON.parse(`{"url":"${base}c","x":1}`).url, base + "c");
    shouldBe(JSON.parse(`{"url":"${base}","x":1}`).url, base);
}

// Shorter long values are compared as two overlapping halves.
for (const length of [17, 18, 24, 30]) {
    const base = prefix.slice(0, length - 1);
    for (let i = 0; i < 20; ++i) {
        const value = base + (i % 3 ? "a" : "b");
        shouldBe(JSON.parse(`{"url":"${value}","x":${i}}`).url, value);
        shouldBe(JSON.parse(`["${value}",${i}]`)[0], value);
    }
}

// A value that needs escapes must not match source text that holds its characters as is.
for (const [escaped, unescaped] of [[`${prefix}a\\"b`, `${prefix}a"b`], [`${prefix}a\\\\x`, `${prefix}a\\x`], [`${prefix}a\\nb`, `${prefix}a\nb`]]) {
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.parse(`{"url":"${escaped}","x":${i}}`).url, unescaped);
    let threw = false;
    try {
        JSON.parse(`{"url":"${unescaped}","x":1}`);
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true);
}

// A value that was escaped only where JSON allows it is the same value as its unescaped text.
{
    const escaped = prefix.replaceAll("/", "\\/") + "slashes.js";
    const unescaped = prefix + "slashes.js";
    for (let i = 0; i < 10; ++i) {
        shouldBe(JSON.parse(`{"url":"${escaped}","x":${i}}`).url, unescaped);
        shouldBe(JSON.parse(`["${escaped}",${i}]`)[0], unescaped);
        shouldBe(JSON.parse(`{"url":"${unescaped}","x":${i}}`).url, unescaped);
    }
}

// Atomizing a parsed value, here by using it as a property key, can replace its characters.
{
    const atomized = prefix + "atomized.js";
    const holder = { [atomized]: 1 };
    for (let i = 0; i < 20; ++i) {
        const parsed = JSON.parse(`{"url":"${atomized}","x":${i}}`).url;
        shouldBe(parsed, atomized);
        shouldBe(holder[parsed], 1);
        shouldBe(JSON.parse(`{"url":"${atomized}","x":${i}}`).url, atomized);
        shouldBe(JSON.parse(`{"url":"${atomized}","\u3042":${i}}`).url, atomized);
        if (i % 5 === 4)
            gc();
    }
}

// More values sharing their first 64 characters than a set holds, of different lengths and suffixes, in turn.
{
    const shared = prefix.repeat(2).slice(0, 64);
    const values = [];
    for (let i = 0; i < 9; ++i)
        values.push(shared + "x".repeat(i % 3) + "/" + i);
    for (let round = 0; round < 6; ++round) {
        const objects = values.map((url, i) => ({ url, i }));
        const parsed = JSON.parse(JSON.stringify(objects));
        for (let i = 0; i < values.length; ++i)
            shouldBe(parsed[i].url, values[i]);
        shouldBe(JSON.parse(`["${values[round]}","${values[8 - round]}"]`)[1], values[8 - round]);
    }
}

// Shorter values that need escapes must not match source text that holds their characters as is.
for (const [escaped, unescaped] of [["abcdefghijklmno\\\"pq", "abcdefghijklmno\"pq"], ["abcdefghijklmno\\\\pq", "abcdefghijklmno\\pq"], ["abcdefghijklmno\\npq", "abcdefghijklmno\npq"]]) {
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.parse(`{"url":"${escaped}","x":${i}}`).url, unescaped);
    let threw = false;
    try {
        JSON.parse(`{"url":"${unescaped}","x":1}`);
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true);
}

// Values of 64 and 65 characters after a cached value with the same first 64, and 64 characters at the end of the input.
{
    const base = prefix.repeat(2);
    const cached = base.slice(0, 70);
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.parse(`{"url":"${cached}","x":${i}}`).url, cached);
    shouldBe(JSON.parse(`{"url":"${base.slice(0, 64)}","x":1}`).url, base.slice(0, 64));
    shouldBe(JSON.parse(`{"url":"${base.slice(0, 65)}","x":1}`).url, base.slice(0, 65));
    let threw = false;
    try {
        JSON.parse(`{"url":"${base.slice(0, 64)}`);
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true);
}

// Values of 64 characters or more that need escapes after their first 64, or that are too long to share.
{
    const base = prefix.repeat(5);
    for (const value of [base.slice(0, 70) + "\"" + base.slice(0, 10), base.slice(0, 70) + "\\" + base.slice(0, 10), base.slice(0, 70) + "\t", base.slice(0, 256), base.slice(0, 257), base.slice(0, 300)]) {
        const text = JSON.stringify({ url: value, x: 1 });
        for (let i = 0; i < 5; ++i)
            shouldBe(JSON.parse(text).url, value);
        shouldBe(JSON.parse(`[${JSON.stringify(value)}]`)[0], value);
    }
}

// A long value cached from 16-bit source is found in a later parse of 8-bit source.
{
    const value = prefix + "from-16-bit-source.js";
    for (let i = 0; i < 5; ++i)
        shouldBe(JSON.parse(`{"\u3042":1,"url":"${value}"}`).url, value);
    for (let i = 0; i < 5; ++i)
        shouldBe(JSON.parse(`{"url":"${value}","x":${i}}`).url, value);
}

// Sloppy JSON, which eval parses, can hold characters as is that strict JSON must escape.
{
    const base = prefix.repeat(2).slice(0, 70);
    for (const [literal, value] of [[`'${base}"sloppy'`, `${base}"sloppy`], [`"${base}\tsloppy"`, `${base}\tsloppy`]]) {
        for (let i = 0; i < 5; ++i)
            shouldBe(eval(`({"url":${literal},"x":${i}})`).url, value);
        let threw = false;
        try {
            JSON.parse(`{"url":"${value}","x":1}`);
        } catch (e) {
            threw = e instanceof SyntaxError;
        }
        shouldBe(threw, true);
    }
}
