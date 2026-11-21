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

let seed = 1;
function random(n) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed % n;
}

const keys = ["a", "b", "children", "data", "x".repeat(29), "k\n", "1"];
function primitive() {
    return [1, "s", 2.5, null, true, "u".repeat(40), [], {}, -3][random(9)];
}
function value(depth) {
    let r = random(10);
    if (depth > 12 || r < 3)
        return primitive();
    if (r < 6)
        return object(depth + 1);
    return array(depth + 1);
}
function object(depth) {
    let result = { };
    for (let i = random(5); i--;)
        result[keys[random(keys.length)]] = value(depth);
    return result;
}
function array(depth) {
    let result = [];
    let allObjects = random(2);
    for (let i = random(5); i--;)
        result.push(allObjects ? object(depth + 1) : value(depth));
    return result;
}

// Objects and arrays of objects as property values, which are read without recursion up to some depth,
// mixed with every other kind of value and element, and repeated so that their keys are cached.
for (let i = 0; i < 500; ++i) {
    let base = object(0);
    let text = JSON.stringify([base, base, base]);
    shouldBe(JSON.stringify(JSON.parse(text)), text, "compact " + i);
    let pretty = JSON.stringify([base, base], null, i % 3);
    shouldBe(JSON.stringify(JSON.parse(pretty)), JSON.stringify([base, base]), "pretty " + i);
}

// Deeper than what is read without recursion.
{
    let deep = 1;
    for (let i = 0; i < 60; ++i)
        deep = i % 3 ? { a: deep, b: 2 } : { c: [{ d: deep }, { d: 3 }], e: [] };
    let text = JSON.stringify([deep, deep]);
    shouldBe(JSON.stringify(JSON.parse(text)), text, "deep");
}

shouldThrow('{"a":{"b":1}', "JSON Parse error: Expected '}'");
shouldThrow('{"a":{"b":1},{"c":2}}', "JSON Parse error: Property name must be a string literal");
shouldThrow('{"a":[{"b":1},]}', "JSON Parse error: Unexpected comma at the end of array expression");
shouldThrow('{"a":[{"b":1}}', "JSON Parse error: Expected ']'");
shouldThrow('{"a":[{"b":1},', "JSON Parse error: Unexpected EOF");
shouldThrow('{"a":[{"b":1},{"b":2},3,]}', "JSON Parse error: Unexpected comma at the end of array expression");
shouldThrow('{"a":[{"b":1}]', "JSON Parse error: Expected '}'");
shouldThrow('{"a":[]', "JSON Parse error: Expected '}'");
shouldThrow('{"a":{"b":{"c":{}}}', "JSON Parse error: Expected '}'");
