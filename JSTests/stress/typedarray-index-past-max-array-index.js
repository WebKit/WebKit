//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault

// Indexed access on a typed array longer than MAX_ARRAY_INDEX elements.
//
// MAX_ARRAY_BUFFER_SIZE is 2^34, so a Uint8Array can be longer than MAX_ARRAY_INDEX (0xFFFFFFFE),
// the ceiling on a property key expressed as an index. Every element below the view's length is a
// valid integer index per IsValidIntegerIndex, so [] must reach it. Such a key arrives as a string,
// where parseIndex() cannot hold it, so it is recognized by isCanonicalNumericIndexString() instead.

function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(`bad value for ${what}: expected ${expected} but got ${actual}`);
}

// 4GiB + 2 bytes: the smallest view with indices on both sides of 0xFFFFFFFF and above 2^32, so one
// allocation covers every case.
let array;
try {
    array = new Uint8Array(4294967298);
} catch (e) {
    // A port that cannot spare 4GiB has nothing to test here.
    if (!(e instanceof RangeError))
        throw e;
}

if (array !== undefined) {
    // fill() and subarray() take the size_t path, so they establish what a byte holds independently of [].
    const trueValueAt = index => array.subarray(index, index + 1)[0];

    for (const index of [
        4294967294, // 0xFFFFFFFE, the last index a property key can express: the control.
        4294967295, // 0xFFFFFFFF: fits a uint32 but is excluded by isIndex().
        4294967296, // 2^32: does not fit a uint32 at all.
        array.length - 1,
    ]) {
        array.fill(7, index, index + 1);
        shouldBe(trueValueAt(index), 7, `fill() at ${index}`);

        shouldBe(array[index], 7, `array[${index}]`);
        shouldBe(array.at(index), 7, `array.at(${index})`);

        array[index] = 99;
        shouldBe(trueValueAt(index), 99, `array[${index}] = 99`);
        shouldBe(array[index], 99, `array[${index}] after assignment`);

        shouldBe(index in array, true, `${index} in array`);
        shouldBe(array.hasOwnProperty(String(index)), true, `hasOwnProperty(${index})`);
        shouldBe(Reflect.has(array, String(index)), true, `Reflect.has(array, "${index}")`);

        const descriptor = Object.getOwnPropertyDescriptor(array, String(index));
        shouldBe(descriptor !== undefined, true, `getOwnPropertyDescriptor(${index}) exists`);
        shouldBe(descriptor.value, 99, `getOwnPropertyDescriptor(${index}).value`);
        shouldBe(descriptor.writable, true, `getOwnPropertyDescriptor(${index}).writable`);
        shouldBe(descriptor.enumerable, true, `getOwnPropertyDescriptor(${index}).enumerable`);
        shouldBe(descriptor.configurable, true, `getOwnPropertyDescriptor(${index}).configurable`);

        // An integer index that exists cannot be deleted.
        shouldBe(delete array[index], false, `delete array[${index}]`);
        shouldBe(trueValueAt(index), 99, `array[${index}] survives delete`);

        Object.defineProperty(array, String(index), { value: 123 });
        shouldBe(trueValueAt(index), 123, `defineProperty at ${index}`);

        // Reading through the prototype must not shadow an element that exists.
        Object.getPrototypeOf(array)[String(index)] = "fromProto";
        shouldBe(array[index], 123, `array[${index}] is not shadowed by the prototype`);
        delete Object.getPrototypeOf(array)[String(index)];

        array.fill(0, index, index + 1);
    }
}
