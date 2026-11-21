function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + actual + ", expected " + expected);
}

// JSON.parse in each global object makes objects with that global object's prototypes, even when the same text was
// parsed in another one before.
const frame = { functionName: "_runTest", url: "https://example.com/resources/benchmark-runner.js", scriptId: "7", lineNumber: 131, columnNumber: 13 };
const nested = { sourceID: "7", name: "", line: 1, column: 2, url: "https://example.com/a.js", expressionLocation: { line: 3, column: 4 }, list: [] };
const items = [];
for (let i = 0; i < 50; ++i)
    items.push({ frames: [frame, frame, frame], value: nested, data: { type: "click" } });
const text = JSON.stringify(items);

const others = [createGlobalObject(), createGlobalObject()];
function check(global, parsed, label) {
    for (const item of parsed) {
        shouldBe(Object.getPrototypeOf(item), global.Object.prototype, label + " item");
        shouldBe(Object.getPrototypeOf(item.data), global.Object.prototype, label + " data");
        shouldBe(Object.getPrototypeOf(item.value), global.Object.prototype, label + " value");
        shouldBe(Object.getPrototypeOf(item.value.expressionLocation), global.Object.prototype, label + " nested");
        shouldBe(Object.getPrototypeOf(item.value.list), global.Array.prototype, label + " empty array");
        shouldBe(Object.getPrototypeOf(item.frames), global.Array.prototype, label + " array");
        for (const element of item.frames)
            shouldBe(Object.getPrototypeOf(element), global.Object.prototype, label + " element");
    }
    shouldBe(JSON.stringify(parsed), text, label + " text");
}
for (let round = 0; round < 3; ++round) {
    check(globalThis, JSON.parse(text), "this " + round);
    for (const [index, other] of others.entries())
        check(other, other.JSON.parse(text), "other " + index + " " + round);
}
