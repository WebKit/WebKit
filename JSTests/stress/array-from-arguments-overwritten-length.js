function testArgumentsWriteLength(...xs) {
    arguments.length = 65535;
    let a = Array.from(arguments);
    if (a.length !== 65535)
        throw new Error("wrong array length");
}

function testArgumentsCursedIndexedGetter(...xs) {
    let getterCalled = false;
    // This test should come after all others because it modifies Object.prototype
    Object.defineProperty(Object.prototype, 3, { get() {
        getterCalled = true;
        return -42;
    }, enumerable: true });
    arguments.length = 65535;
    let a = Array.from(arguments);
    if (a.length !== 65535)
        throw new Error("wrong array length");
    if (!getterCalled || a[3] !== -42)
        throw new Error("wrong array value");
}

function testArgumentsCursedIndexedGetterThrows(...xs) {
    Object.defineProperty(Object.prototype, 4, { get() {
        throw -43;
    }, enumerable: true });
    arguments.length = 65535;
    let caughtException;
    try {
        Array.from(arguments);
    } catch (e) {
        caughtException = e;
    }
    if (caughtException !== -43)
        throw new Error("didn't throw");
}


testArgumentsWriteLength({});
testArgumentsCursedIndexedGetter({});
testArgumentsCursedIndexedGetterThrows({});
