function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + actual + ", expected " + expected);
}

function shouldThrow(text, message) {
    let error = null;
    try {
        JSON.parse(text);
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof SyntaxError, true, "throws for " + text);
    shouldBe(error.message, message, "message for " + text);
}

// Objects that follow each other in an array, with cached keys, keys that the cache misses, empty objects,
// nested arrays and objects, and whitespace between them.
for (let i = 0; i < 100; ++i) {
    let items = [];
    for (let j = 0; j < 50; ++j) {
        switch ((i + j) % 6) {
        case 0:
            items.push({ a: j, b: "x" + j });
            break;
        case 1:
            items.push({});
            break;
        case 2:
            items.push({ ["k" + (i * 50 + j)]: j, a: j });
            break;
        case 3:
            items.push({ a: [{ b: j }, { b: j + 1 }], c: { d: [{}, { e: j }] } });
            break;
        case 4:
            items.push({ a: j, b: "x", c: 1.5, d: true, e: null, f: [], g: 7, h: 8 });
            break;
        case 5:
            items.push([{ a: j }, { a: j }]);
            break;
        }
    }
    let text = JSON.stringify(items);
    shouldBe(JSON.stringify(JSON.parse(text)), text, "compact " + i);
    let pretty = JSON.stringify(items, null, i % 3);
    shouldBe(JSON.stringify(JSON.parse(pretty)), text, "pretty " + i);
}

// An object followed by a comma and another object is only valid in an array.
shouldThrow('{"a":1},{"b":2}', "JSON Parse error: Unable to parse JSON string");
shouldThrow('{"x":{"a":1},{"b":2}}', "JSON Parse error: Property name must be a string literal");
shouldThrow('[{"a":1},{"a":1},', "JSON Parse error: Unexpected EOF");
shouldThrow('[{"a":1},{', "JSON Parse error: Expected '}'");
shouldThrow('[{"a":1},{"a', "JSON Parse error: Unterminated string");
shouldThrow('[{"a":1},{"a":1}', "JSON Parse error: Expected ']'");
shouldThrow('[{"a":1},{"a":1},]', "JSON Parse error: Unexpected comma at the end of array expression");
