function assert(a, message) {
    if (!a)
        throw new Error(message);
}

try {
    const var_1 = 'a'.padStart(2147483648 - 1);
    new var_1();
    assert(false, `Should throw OOM error`);
} catch (error) {
    assert(error.message == "Out of memory", "Expected OutOfMemoryError, but got: " + error);
}
