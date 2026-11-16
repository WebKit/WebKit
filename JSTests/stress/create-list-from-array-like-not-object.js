function testValid(value) {
    const foo = {x: 0};
    foo.__proto__ = new Proxy({}, { ownKeys() { return value; } });
    for (const x in foo) { }
}

testValid({});
testValid([]);
testValid(["x", Symbol("y")]);
testValid({ length: 1, 0: 'x' });

function testInvalid(value) {
    try {
        testValid(value);
        throw new Error('should have thrown');
    } catch (err) {
        if (err.message !== "Proxy handler's 'ownKeys' method must return an object")
            throw new Error("Expected createListFromArrayLike error");
    }
}

testInvalid(true);
testInvalid(false);
testInvalid(null);
testInvalid(0);
