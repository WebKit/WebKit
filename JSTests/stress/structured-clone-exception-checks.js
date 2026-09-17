//@ runDefault("--validateExceptionChecks=true")

// Structured serialization runs arbitrary JS through getters and rope resolution, so every step
// of the walk has to check for a pending exception. --validateExceptionChecks turns a missed
// check into a crash.

function shouldThrow(func, expectedMessage) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected an exception, but none was thrown");
    if (error.message !== expectedMessage)
        throw new Error("Expected '" + expectedMessage + "' but got '" + error.message + "'");
}

function serialize(value) {
    $.agent.broadcast(value);
}

for (let value of [
    undefined, null, true, 0, 1, 1.5, "", "hello", "a".repeat(200) + "!",
    new String("boxed"), new String("a".repeat(200) + "!"), new Number(1), new Boolean(false),
    1n, 2n ** 128n, new Date(0), /pattern/gu, new Error("plain"), new TypeError("typed"),
    [1, 2, 3], new Map([[1, "one"]]), new Set(["a"]),
    new Uint8Array([1, 2]), new DataView(new ArrayBuffer(4)), new ArrayBuffer(4),
    { a: 1, b: { c: [new Error("nested")] } },
])
    serialize(value);

// An Error's name is read with a full [[Get]], so an accessor there throws from inside the
// serializer's terminal-value handling rather than from the property walk.
function errorWithThrowingName() {
    let error = new Error("outer");
    Object.defineProperty(error, "name", { configurable: true, get() { throw new Error("name getter"); } });
    return error;
}

shouldThrow(() => serialize(errorWithThrowingName()), "name getter");
shouldThrow(() => serialize({ a: errorWithThrowingName(), b: 1 }), "name getter");
shouldThrow(() => serialize([errorWithThrowingName()]), "name getter");
shouldThrow(() => serialize(new Map([["k", errorWithThrowingName()]])), "name getter");
shouldThrow(() => serialize(new Set([errorWithThrowingName()])), "name getter");

shouldThrow(() => serialize({ get x() { throw new Error("value getter"); } }), "value getter");
