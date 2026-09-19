function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrowTypeError(func) {
    try {
        func();
    } catch (error) {
        if (error instanceof TypeError)
            return;
        throw new Error(`bad error: ${String(error)}`);
    }
    throw new Error(`should throw a TypeError`);
}

var otherRealm = createGlobalObject();
otherRealm.eval(`var buffer = new ArrayBuffer(16);`);

for (var i = 0; i < 1e3; ++i) {
    // SpeciesConstructor reads the receiver's own "constructor", so a pristine
    // buffer from another realm slices into that realm, not into this one.
    shouldBe(ArrayBuffer.prototype.slice.call(otherRealm.buffer, 0, 8).constructor, otherRealm.ArrayBuffer);

    var extended = new otherRealm.ArrayBuffer(16);
    extended.ownProperty = 1;
    shouldBe(ArrayBuffer.prototype.slice.call(extended, 0, 8).constructor, otherRealm.ArrayBuffer);

    shouldBe(new ArrayBuffer(16).slice(0, 8).constructor, ArrayBuffer);
}

class Subclass extends ArrayBuffer { }

var speciesDescriptor = Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species);
Object.defineProperty(ArrayBuffer, Symbol.species, {
    configurable: true,
    get() { return Subclass; }
});

for (var i = 0; i < 1e3; ++i) {
    shouldBe(new ArrayBuffer(16).slice(0, 8).constructor, Subclass);

    var withOwnProperty = new ArrayBuffer(16);
    withOwnProperty.ownProperty = 1;
    shouldBe(withOwnProperty.slice(0, 8).constructor, Subclass);

    var withOwnConstructor = new ArrayBuffer(16);
    withOwnConstructor.constructor = ArrayBuffer;
    shouldBe(withOwnConstructor.slice(0, 8).constructor, Subclass);
}

Object.defineProperty(ArrayBuffer, Symbol.species, speciesDescriptor);

for (var i = 0; i < 1e3; ++i) {
    shouldBe(new ArrayBuffer(16).slice(0, 8).constructor, ArrayBuffer);

    var buffer = new ArrayBuffer(16);
    buffer.constructor = { [Symbol.species]: null };
    shouldBe(buffer.slice(0, 8).constructor, ArrayBuffer);

    buffer = new ArrayBuffer(16);
    buffer.constructor = undefined;
    shouldBe(buffer.slice(0, 8).constructor, ArrayBuffer);

    buffer = new ArrayBuffer(16);
    buffer.constructor = null;
    shouldThrowTypeError(() => buffer.slice(0, 8));

    buffer = new ArrayBuffer(16);
    buffer.constructor = { [Symbol.species]: {} };
    shouldThrowTypeError(() => buffer.slice(0, 8));
}
