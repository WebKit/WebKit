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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldThrow(() => JSON.parse(`{"object": 42`), `SyntaxError: JSON Parse error: Expected '}'`);
shouldBe(JSON.stringify(JSON.parse(`{}`)), `{}`);
shouldBe(JSON.stringify(JSON.parse(`{"object": {}}`)), `{"object":{}}`);
shouldThrow(() => JSON.parse(`{"object": 42, "test":{}`), `SyntaxError: JSON Parse error: Expected '}'`);
shouldBe(JSON.stringify(JSON.parse(`{"hello":42, "object": {}}`)), `{"hello":42,"object":{}}`);
shouldBe(JSON.stringify(JSON.parse(`{"hello":42, "object": 42, "test": 43}`)), `{"hello":42,"object":42,"test":43}`);
