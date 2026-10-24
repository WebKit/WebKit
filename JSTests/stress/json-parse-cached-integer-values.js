function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

// JSONCache matches a key or a string value only when at least 32 characters of source remain.
const padding = " ".repeat(40);

// The first parse fills JSONCache and the later ones read keys and values from it. Every GC clears it.
function parse(text) {
    let result;
    for (let i = 0; i < 3; ++i)
        result = JSON.parse(text);
    return result;
}

function shouldThrow(text, message) {
    for (let i = 0; i < 3; ++i) {
        let error = null;
        try {
            JSON.parse(text);
        } catch (e) {
            error = e;
        }
        if (!(error instanceof SyntaxError))
            throw new Error(`did not throw SyntaxError: ${text}`);
        if (message && !error.message.includes(message))
            throw new Error(`bad error message: ${error.message}, expected: ${message}`);
    }
}

const numbers = [
    "0", "7", "10", "123456789", "999999999", "1000000000", "1234567890", "2147483647", "2147483648",
    "4294967295", "4294967296", "99999999999999999999", "1.5", "1e5", "1E5", "1e-5", "0.0", "-0", "-1", "-123",
];
const invalid = ["00", "01", "1.", "1e", "1a", "1-", "-", "-a", "0x1", "1.e5"];
const beforeComma = [",", " ,", "\n,", "\t,"];
const beforeBrace = ["}", " }", "\n}", "\t}"];
// The second prefix holds a character outside Latin-1, which makes the source 16-bit.
const prefixes = [["", undefined], ['"p":"\u3042",', "\u3042"]];
const cached = "cached";
const cachedLong = "q".repeat(30);

const iterations = Math.min(testLoopCount, 2);
for (let iteration = 0; iteration < iterations; ++iteration) {
    for (const [prefix, p] of prefixes) {
        for (const number of numbers) {
            const expected = Number(number);
            for (const separator of beforeComma) {
                const result = parse(`{${prefix}"a":${number}${separator}"b":2}${padding}`);
                shouldBe(result.p, p);
                shouldBe(result.a, expected);
                shouldBe(result.b, 2);
            }
            for (const separator of beforeBrace) {
                const result = parse(`{${prefix}"a":1,"b":${number}${separator}${padding}`);
                shouldBe(result.p, p);
                shouldBe(result.a, 1);
                shouldBe(result.b, expected);
            }
            shouldBe(parse(`{${prefix}"a": ${number}, "b": ${number}\n}${padding}`).b, expected);
        }

        for (const number of invalid) {
            for (const separator of beforeComma)
                shouldThrow(`{${prefix}"a":${number}${separator}"b":2}${padding}`);
            for (const separator of beforeBrace)
                shouldThrow(`{${prefix}"a":1,"b":${number}${separator}${padding}`);
        }

        shouldThrow(`{${prefix}"a":1,}${padding}`, "Property name must be a string literal");
        shouldThrow(`{${prefix}"a":1, }${padding}`, "Property name must be a string literal");
        shouldThrow(`{${prefix}"a":"${cached}",}${padding}`, "Property name must be a string literal");
        shouldThrow(`{${prefix}"a":1,"b":2,}${padding}`, "Property name must be a string literal");
        shouldThrow(`{${prefix}"a":1,"b":12`);
        shouldThrow(`{${prefix}"a":1,"b":12,`);
        shouldThrow(`{${prefix}"a":1,"b":12}]`);
    }

    // String values found in JSONCache, followed by whitespace, by a wrong token, and by the end of the input.
    shouldBe(parse(`{"a":"${cached}","b":2}${padding}`).a, cached);
    shouldBe(parse(`{"a":"${cached}" ,"b":2}${padding}`).a, cached);
    shouldBe(parse(`{"a":"${cached}"\n,"b":2}${padding}`).a, cached);
    shouldBe(parse(`{"a":1,"b":"${cached}"\t}${padding}`).b, cached);
    shouldBe(parse(`{"a": "${cached}", "b": "${cached}"}${padding}`).b, cached);
    shouldThrow(`{"a":"${cached}":"b"}${padding}`);
    shouldThrow(`{"a":"${cached}" "b":2}${padding}`);
    shouldThrow(`{"a":"${cached}"]${padding}`);
    shouldThrow(`{"a":"${cached}"1}${padding}`);
    shouldBe(parse(`{"a":"${cachedLong}"}${padding}`).a, cachedLong);
    shouldThrow(`{"a":"${cachedLong}"`);
    shouldThrow(`{"a":"${cachedLong}",`);
    shouldThrow(`{"a":"${cachedLong}" `);

    // Enough properties of one shape to move past inline storage and grow the out-of-line storage
    // several times, with each value read from the cursor and the objects kept alive across GCs.
    const keyCount = 40;
    const wide = `{${Array.from({ length: keyCount }, (_, i) => `"k${i}":${i % 3 ? i : `"${cached}"`}`).join(",")}}${padding}`;
    const wideObjects = [];
    for (let i = 0; i < 20; ++i) {
        wideObjects.push(JSON.parse(wide));
        if (i % 5 === 1)
            edenGC();
        if (i % 5 === 3)
            fullGC();
    }
    for (const object of wideObjects) {
        shouldBe(Object.keys(object).length, keyCount);
        for (let i = 0; i < keyCount; ++i)
            shouldBe(object[`k${i}`], i % 3 ? i : cached);
    }

    // Nested values add their own names and transitions to JSONCache, which can evict the entries of the
    // object around them, and later keys of that object must still be read correctly.
    for (let i = 0; i < 20; ++i) {
        const nestedObject = `{${Array.from({ length: 16 }, (_, j) => `"n${iteration}_${i}_${j}":${j}`).join(",")}}`;
        const nestedArray = `[${Array.from({ length: 8 }, (_, j) => `{"m${i}_${j}":"${cached}","z":${j}}`).join(",")}]`;
        const text = `{"a":1,"o":${nestedObject},"b":2,"r":${nestedArray},"c":"${cached}","d":3}${padding}`;
        const result = parse(text);
        shouldBe(result.a, 1);
        shouldBe(result.o[`n${iteration}_${i}_15`], 15);
        shouldBe(result.b, 2);
        shouldBe(result.r.length, 8);
        shouldBe(result.r[7][`m${i}_7`], cached);
        shouldBe(result.r[7].z, 7);
        shouldBe(result.c, cached);
        shouldBe(result.d, 3);
    }

    gc();
}
