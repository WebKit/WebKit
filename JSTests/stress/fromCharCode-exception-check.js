// This shouldn't crash.

try {
    String.fromCharCode(Symbol(), new Proxy({}, { get() { } }));
} catch (e) {
    if (!(e instanceof TypeError) || e.message !== "Cannot convert a symbol to a number")
        throw new Error("bad error type or message" + e);
}
