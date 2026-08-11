function assert(ok, message = '') {
    if (!ok)
        throw new Error(`Assertion!: ${message}`);
}

function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName)
        throw new Error(`must specify test case name`);

    const expected = `${expectedErrorCtor.name}(${expectedErrorMessage})`;
    try {
        fn();
        throw new Error(`${caseName}: Expected to throw ${expected}, but succeeded`);
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        if (!(e instanceof expectedErrorCtor) || e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expected} but got ${actual}`);
    }
}

const newTarget = Object.defineProperty(function () { }.bind(), "prototype", {
    get() {
        throw new Error("GetPrototypeFromConstructor not executed");
    }
});

class ExpectedError extends Error {
    constructor(...args) {
        super(...args);
        this.name = new.target.name;
    }
}

const TEST_TARGET = [
    DataView,
];

for (const ctor of TEST_TARGET) {
    const label = ctor.name;

    // Zero length buffer.
    const ab = new ArrayBuffer(0);

    const byteLength = 10;
    assert(byteLength > ab.byteLength, "byteLength is larger than the buffer length");

    // https://tc39.es/ecma262/2026/#sec-dataview-constructor
    // The step 9-a |ToIndex(byteLength)| should happens before the step 10 |OrdinaryCreateFromConstructor|.
    shouldThrow(label, () => {
        Reflect.construct(ctor, [ab, 0, byteLength], newTarget);
    }, RangeError, 'Length out of range of buffer');
}