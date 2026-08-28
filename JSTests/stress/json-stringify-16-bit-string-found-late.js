function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function reference(value, space) {
    return JSON.stringify(value, (key, value) => value, space);
}

function makeBody(prefixLength, wideString) {
    let messages = [];
    let produced = 0;
    for (let i = 0; produced < prefixLength; ++i) {
        let text = "x".repeat(100 + (i * 37) % 700);
        messages.push({ role: i & 1 ? "assistant" : "user", content: [{ type: "text", text }], index: i });
        produced += text.length + 60;
    }
    messages.push({ role: "user", content: [{ type: "text", text: wideString }], index: messages.length });
    messages.push({ role: "assistant", content: [{ type: "text", text: "y".repeat(500) }], index: messages.length });
    return { model: "m", max_tokens: 4096, stream: true, messages };
}

const wideStrings = [
    "\u3042",
    "abc\u3042def",
    "\u2014 \u3042\u3044\u3046 \u00e9",
    "\ud83d\ude00",
    "\ud800",
    "\udc00\ud800",
    "line\n\u3042\ttab\"quote\\backslash ",
    "\u3042".repeat(3000),
];

for (let prefixLength of [0, 1000, 5000, 7000, 9000]) {
    for (let wideString of wideStrings) {
        let body = makeBody(prefixLength, wideString);
        for (let space of [undefined, 2, "\t"])
            shouldBe(JSON.stringify(body, undefined, space), reference(body, space));
    }
}

for (let prefixLength of [20000, 60000]) {
    let body = makeBody(prefixLength, wideStrings[2]);
    shouldBe(JSON.stringify(body), reference(body));
    shouldBe(JSON.stringify(body, undefined, 2), reference(body, 2));
}

for (let prefixLength of [5000, 20000]) {
    let array = [];
    for (let i = 0; i < prefixLength / 4; ++i)
        array.push(i);
    array.push([{ wide: "\u3042" }]);
    shouldBe(JSON.stringify(array), reference(array));
    shouldBe(JSON.stringify(array, undefined, 1), reference(array, 1));
}

{
    let object = { long: "z".repeat(20000), wide: "\u3042", toJSONHolder: { toJSON() { return "replaced"; } } };
    shouldBe(JSON.stringify(object), reference(object));
    shouldBe(JSON.stringify(object, undefined, 2), reference(object, 2));
}
