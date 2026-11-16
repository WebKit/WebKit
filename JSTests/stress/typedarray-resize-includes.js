function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error(`actual: ${actual}, expected: ${expected}`);
    }
}

{
    var arraybuffer = new ArrayBuffer(4, { maxByteLength: 20 });
    var int8array = new Int8Array(arraybuffer);
    var index = {
        valueOf() {
            arraybuffer.resize(0);
            return 10;
        },
    };
    shouldBe(int8array.length, 4);
    var result = int8array.includes(undefined, index);
    shouldBe(int8array.length, 0);
    shouldBe(result, false);
}

{
    var arraybuffer = new ArrayBuffer(4, { maxByteLength: 20 });
    var byteOffset = 1; // Uses byteOffset to make typed array out-of-bounds when shrinking size to zero.
    var int8array = new Int8Array(arraybuffer, byteOffset);
    var index = {
        valueOf() {
            arraybuffer.resize(0);
            return 10;
        },
    };
    shouldBe(int8array.length, 3);
    var result = int8array.includes(undefined, index);
    shouldBe(int8array.length, 0);
    shouldBe(result, false);
}

{
    var arraybuffer = new ArrayBuffer(2, { maxByteLength: 20 });
    var intarray = new Int8Array(arraybuffer);
    intarray[0] = 21;
    intarray[1] = 31;
    shouldBe(intarray.length, 2);
    arraybuffer.resize(1);
    shouldBe(intarray.length, 1);
    shouldBe(intarray.includes(21), true);
    shouldBe(intarray.includes(31), false);
    arraybuffer.resize(0);
    shouldBe(intarray.length, 0);
    shouldBe(intarray.includes(21), false);
    shouldBe(intarray.includes(31), false);
}
