//@ requireOptions("--useJSONSourceTextAccess=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(() => {
    JSON.rawJSON("42: 42");
}, `SyntaxError: JSON Parse error: Unexpected content at end of JSON literal`);
shouldThrow(() => {
    JSON.rawJSON("42n");
}, `SyntaxError: JSON Parse error: Unexpected content at end of JSON literal`);
shouldThrow(() => {
    JSON.rawJSON("[]");
}, `SyntaxError: JSON Parse error: Could not parse value expression`);
shouldThrow(() => {
    JSON.rawJSON("{}");
}, `SyntaxError: JSON Parse error: Could not parse value expression`);
shouldThrow(() => {
    JSON.rawJSON("'string'");
}, `SyntaxError: JSON Parse error: Single quotes (') are not allowed in JSON`);
shouldThrow(() => {
    JSON.rawJSON("Hello");
}, `SyntaxError: JSON Parse error: Unexpected identifier "Hello"`);

var texts = [
    '42',
    'null',
    'false',
    'true',
    '"Hello"',
];

for (let text of texts) {
    let object = JSON.rawJSON(text);
    shouldBe(typeof object, "object");
    shouldBe(JSON.isRawJSON(object), true);
    shouldBe(Object.isFrozen(object), true);
    shouldBe(Object.getPrototypeOf(object), null);
    shouldBe(object.rawJSON, text);
}
shouldBe(JSON.isRawJSON({}), false);
shouldBe(JSON.isRawJSON({rawJSON:"hello"}), false);
