// The original is https://github.com/tc39/test262/blob/d0c1b4555b03dd404873fd6422a4b5da00136500/test/built-ins/DataView/byteOffset-validated-against-initial-buffer-length.js

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

const newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
  get() {
    throw new Error("GetPrototypeFromConstructor not executed");
  }
});

const TEST_TARGET = [
    DataView,
];

for (const ctor of TEST_TARGET) {
    const label = ctor.name;

    // Zero length buffer.
    const ab = new ArrayBuffer(0);
    // Byte offset is larger than the buffer length, which is zero.
    const byteOffset = 10;
    
    // https://tc39.es/ecma262/2026/#sec-dataview-constructor
    // The step 3 |ToIndex(byteOffset)| should happens before the step 10 |OrdinaryCreateFromConstructor|.
    shouldThrow(label, () => {
        Reflect.construct(ctor, [ab, byteOffset], newTarget);
    }, RangeError, 'byteOffset exceeds source ArrayBuffer byteLength');
}