function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(func) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = error instanceof SyntaxError;
    }
    if (!threw)
        throw new Error("did not throw SyntaxError");
}

function roundTrip(text) {
    return JSON.stringify(JSON.parse(text));
}

const padding = " ".repeat(40);
const names = [
    "a",
    "ab",
    "lineNumber",
    "expressionLocation",
    "x".repeat(28),
    "y".repeat(29),
    "z".repeat(30),
    "\u00e9t\u00e9",
];

const iterations = Math.min(testLoopCount, 50);
for (let iteration = 0; iteration < iterations; ++iteration) {
    for (const name of names) {
        const expected = JSON.stringify({ first: 1, [name]: 2, last: 3 });
        const quoted = JSON.stringify(name);

        shouldBe(roundTrip(`{"first":1,${quoted}:2,"last":3}${padding}`), expected);
        shouldBe(roundTrip(`{"first":1,${quoted}:2,"last":3}`), expected);
        shouldBe(roundTrip(`{"first":1,${quoted} :2,"last":3}${padding}`), expected);
        shouldBe(roundTrip(`{"first":1, ${quoted}:2,"last":3}${padding}`), expected);
        shouldBe(roundTrip(`{"first": 1, ${quoted}: 2, "last": 3}${padding}`), expected);
        shouldBe(roundTrip(`{\n  "first": 1,\n  ${quoted}: 2,\n\t"last": 3\r\n}${padding}`), expected);
        shouldBe(roundTrip(`{ "first":1 , ${quoted}:2 ,"last":3 }${padding}`), expected);
        shouldBe(JSON.stringify(JSON.parse(`{"first": 1, ${quoted}: 2, "last": 3}${padding}`, (key, value) => value)), expected);

        const escaped = quoted.slice(0, 1) + "\\u" + name.charCodeAt(0).toString(16).padStart(4, "0") + quoted.slice(2);
        shouldBe(roundTrip(`{"first":1,${escaped}:2,"last":3}${padding}`), expected);

        const different = JSON.stringify({ first: 1, [name + "q"]: 2, last: 3 });
        shouldBe(roundTrip(`{"first":1,${JSON.stringify(name + "q")}:2,"last":3}${padding}`), different);
        if (name.length > 1) {
            const shorter = JSON.stringify({ first: 1, [name.slice(0, -1)]: 2, last: 3 });
            shouldBe(roundTrip(`{"first":1,${JSON.stringify(name.slice(0, -1))}:2,"last":3}${padding}`), shorter);
        }

        shouldThrow(() => JSON.parse(`{"first":1,${quoted}2,"last":3}${padding}`));
        shouldThrow(() => JSON.parse(`{"first":1,${quoted}:`));
        shouldThrow(() => JSON.parse(`{"first":1,${quoted}`));
    }

    const quoteInName = JSON.stringify({ first: 1, 'a"b': 2 });
    shouldBe(roundTrip(`{"first":1,"a\\"b":2}${padding}`), quoteInName);

    const backslashInName = JSON.stringify({ first: 1, "a\\b": 2 });
    shouldBe(roundTrip(`{"first":1,"a\\\\b":2}${padding}`), backslashInName);

    const siblings = ["line", "lineNumber", "linear", "li", "l", "lineNumbers", "column", "columnNumber"];
    for (let i = 0; i < siblings.length; ++i) {
        const name = siblings[(i + iteration) % siblings.length];
        const expected = JSON.stringify({ [name]: i, next: [i] });
        shouldBe(roundTrip(`{${JSON.stringify(name)}:${i},"next":[${i}]}${padding}`), expected);
        shouldBe(roundTrip(`{${JSON.stringify(name)} :${i},"next":[${i}]}${padding}`), expected);
    }

    if (iteration % 10 === 0)
        gc();
}
