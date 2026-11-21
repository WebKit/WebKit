function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + actual + ", expected " + expected);
}

// Objects whose text repeats, with every kind of value, nested in objects and in arrays.
const frames = [
    { functionName: "", url: "https://example.com/resources/benchmark-runner.js", scriptId: "7", lineNumber: 256, columnNumber: 22 },
    { functionName: "_runTest", url: "https://example.com/resources/benchmark-runner.js", scriptId: "7", lineNumber: 131, columnNumber: 13 },
    { functionName: "click", url: "[native code]", scriptId: "0", lineNumber: 0, columnNumber: 0 },
    { type: "click" },
    { a: 0, b: 1.5, c: true, d: false, e: null, f: "x\ny" },
    { a: "\u00e9t\u00e9", b: "a".repeat(40), c: -2147483648, d: 1e300 },
    { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 },
    { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7 },
    { },
    { k: "v".repeat(300) },
    { sourceID: "7", name: "", line: 1, column: 2, url: "https://example.com/a.js", expressionLocation: { line: 3, column: 4 } },
    { a: 1, b: [], c: { } },
    { a: { }, b: { x: 1 }, c: [] },
    { a: [1], b: 2 },
    { a: { b: { c: 1 } }, d: 2 },
    { a: { x: "y", z: [] }, b: 1 },
];
function makeEvent(i) {
    return {
        startTime: i,
        data: frames[i % frames.length],
        stackTrace: [frames[(i + 1) % frames.length], frames[(i + 2) % frames.length], frames[i % 3]],
        children: i % 2 ? [] : [{ data: frames[(i + 3) % frames.length], children: [] }],
        type: "EventDispatch",
    };
}
const events = [];
for (let i = 0; i < 2000; ++i)
    events.push(makeEvent(i));
const text = JSON.stringify(events);

let previous = null;
for (let round = 0; round < 6; ++round) {
    const parsed = JSON.parse(text);
    shouldBe(JSON.stringify(parsed), text, "round " + round);
    // Every object is a distinct ordinary object.
    shouldBe(parsed[3].data === parsed[13].data, false, "identity " + round);
    shouldBe(Object.getPrototypeOf(parsed[3].data), Object.prototype, "prototype " + round);
    const withNested = parsed.filter((event) => event.data.expressionLocation);
    shouldBe(withNested.length > 2, true, "nested " + round);
    shouldBe(withNested[0].data.expressionLocation === withNested[1].data.expressionLocation, false, "nested identity " + round);
    const withArray = parsed.filter((event) => Array.isArray(event.data.b));
    shouldBe(withArray[0].data.b === withArray[1].data.b, false, "array identity " + round);
    shouldBe(Array.isArray(withArray[0].data.b) && withArray[0].data.b.length, 0, "empty array " + round);
    if (previous) {
        shouldBe(previous[0].stackTrace[0] === parsed[0].stackTrace[0], false, "identity across parses " + round);
        // Changing objects from an earlier parse does not change what a later parse makes.
        previous[1].data.functionName = "changed";
        previous[1].data.extra = 1;
        delete previous[2].data.url;
        Object.freeze(previous[3].data);
        previous[0].stackTrace[0].lineNumber = -1;
        previous[0].stackTrace.push(1);
        for (const event of previous.slice(0, 40)) {
            for (const frame of [event.data, ...event.stackTrace]) {
                if (frame && frame.expressionLocation)
                    frame.expressionLocation.line = "changed";
                if (frame && Array.isArray(frame.b))
                    frame.b.push("changed");
                if (frame && frame.c && typeof frame.c === "object")
                    frame.c.changed = true;
            }
        }
    }
    previous = parsed;
    if (round % 2)
        gc();
}

// Negative zero, which only the lexer reads, among values that the parser reads without it.
const negativeZero = '{"a":-0,"b":"padding to make the object long enough","c":-7,"d":true,"e":false,"f":null}';
for (const source of ["[" + [negativeZero, negativeZero, negativeZero].join() + "]", '{"o":[' + [negativeZero, negativeZero, negativeZero].join() + '],"p":' + negativeZero + ',"q":' + negativeZero + "}"]) {
    for (let i = 0; i < 3; ++i) {
        const parsed = JSON.parse(source);
        for (const object of Array.isArray(parsed) ? parsed : [...parsed.o, parsed.p, parsed.q]) {
            shouldBe(Object.is(object.a, -0), true, "negative zero " + i);
            shouldBe(object.c, -7, "negative integer " + i);
            shouldBe(object.d, true, "true " + i);
            shouldBe(object.e, false, "false " + i);
            shouldBe(object.f, null, "null " + i);
        }
    }
}
for (const value of ["-1", "-2147483647", "-999999999", "-1000000000", "-2147483648", "-2147483649", "-0.5", "-1.0", "-1e3", "-0.0", "true", "false", "null"]) {
    const object = '{"a":' + value + ',"b":"padding to make the object long enough"}';
    const source = '{"o":[' + [object, object, object].join() + "]}";
    const parsed = JSON.parse(source);
    for (const element of parsed.o)
        shouldBe(Object.is(element.a, JSON.parse(value)), true, "value " + value);
}

// Text that repeats an object near the end of the source, with less left after it than a lookup reads.
for (let length = 0; length < 50; ++length) {
    const tail = '{"a":1,"b":"' + "x".repeat(length) + '"}';
    const source = "[" + tail + "," + tail + "," + tail + "]";
    shouldBe(JSON.stringify(JSON.parse(source)), source, "tail " + length);
    const value = '{"o":' + tail + ',"p":' + tail + ',"q":' + tail + '}';
    shouldBe(JSON.stringify(JSON.parse(value)), value, "value tail " + length);
    const element = '{"o":[' + tail + "," + tail + "," + tail + "]}";
    shouldBe(JSON.stringify(JSON.parse(element)), element, "element tail " + length);
}

// The same prefix with different endings, longer than a lookup reads, and errors right after a repeated object.
const prefix = '{"functionName":"f","url":"https://example.com/a.js","scriptId":"1","lineNumber":';
for (let i = 0; i < 100; ++i) {
    const objects = [prefix + (i % 7) + "}", prefix + (i % 7) + "}", prefix + (i % 5) + "}", prefix + (i % 3) + ',"x":1}', prefix + (i % 7) + "}"];
    for (const source of ["[" + objects.join() + "]", '{"o":[' + objects.join() + "]}", "{" + objects.map((object, index) => '"k' + index + '":' + object).join() + "}"])
        shouldBe(JSON.stringify(JSON.parse(source)), source, "prefix " + i);
}
for (const source of ['[{"a":1},{"a":1},{"a":1}', '[{"a":1},{"a":1},{"a":1},]', '{"x":{"a":1},"y":{"a":1},"z":{"a":1}', '{"x":{"a":1},"y":{"a":1},"z":{"a":1}x}',
    '{"o":[{"a":-01}]}', '{"o":[{"a":-}]}', '{"o":[{"a":-x}]}', '{"o":[{"a":truex}]}', '{"o":[{"a":nul}]}', '{"o":[{"a":fals}]}', '{"o":[{"a":t', '{"o":[{"a":-',
    '{"o":[{"a":true]}', '{"o":[{"a":null "b":1}]}']) {
    let threw = false;
    try {
        JSON.parse(source);
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true, "error " + source);
}
