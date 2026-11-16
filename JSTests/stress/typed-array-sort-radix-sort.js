function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

var seed = 1;
function nextRandom() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed |= 0;
    return seed >>> 8;
}

function isBigIntArray(ctor) {
    return ctor === BigInt64Array || ctor === BigUint64Array;
}

function compareNumbers(a, b) {
    return a - b;
}

function compareBigInts(a, b) {
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

function sortedReference(array) {
    return Array.from(array).sort(isBigIntArray(array.constructor) ? compareBigInts : compareNumbers);
}

function shouldMatchReference(array) {
    var expected = sortedReference(array);
    shouldBe(array.sort(), array);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(array[i], expected[i]);
}

function coerce(ctor, value) {
    return isBigIntArray(ctor) ? BigInt(value) : value;
}

var integerConstructors = [Int16Array, Uint16Array, Int32Array, Uint32Array, BigInt64Array, BigUint64Array];

// Straddle every length at which the engine switches to radix sort, from well below the lowest to
// above the highest.
var lengths = [0, 1, 2, 8, 64, 255, 256, 257, 511, 512, 513, 1023, 1024, 1025, 1100];

var integerPatterns = {
    random: function (i, length) { return nextRandom(); },
    identical: function (i, length) { return 42; },
    ascending: function (i, length) { return i; },
    descending: function (i, length) { return length - i; },
    // Only the low byte varies, so every higher digit holds one value and is skipped.
    lowDigitOnly: function (i, length) { return nextRandom() & 0xff; },
    // Only the second byte varies, so the lowest digit is skipped instead.
    secondDigitOnly: function (i, length) { return (nextRandom() & 0xff) * 0x100; },
    twoValues: function (i, length) { return (i % 2) ? 30000 : 7; },
    alternatingSign: function (i, length) { return (i % 2) ? -nextRandom() : nextRandom(); },
};

for (var ctor of integerConstructors) {
    for (var length of lengths) {
        for (var name in integerPatterns) {
            var array = new ctor(length);
            for (var i = 0; i < length; ++i)
                array[i] = coerce(ctor, integerPatterns[name](i, length));
            shouldMatchReference(array);
        }
    }
}

// Extreme values must land at the ends, which is where a wrong sign bit flip shows up.
var extremes = new Map([
    [Int16Array, [-32768, 32767]],
    [Uint16Array, [0, 65535]],
    [Int32Array, [-2147483648, 2147483647]],
    [Uint32Array, [0, 4294967295]],
    [BigInt64Array, [-9223372036854775808n, 9223372036854775807n]],
    [BigUint64Array, [0n, 18446744073709551615n]],
]);

for (var ctor of integerConstructors) {
    var bounds = extremes.get(ctor);
    var low = bounds[0];
    var high = bounds[1];
    var array = new ctor(2048);
    for (var i = 0; i < array.length; ++i)
        array[i] = (i % 3 === 0) ? low : ((i % 3 === 1) ? high : coerce(ctor, 0));
    shouldMatchReference(array);
    shouldBe(array[0], low);
    shouldBe(array[array.length - 1], high);
}

// toSorted takes the same no-comparator path, and must not disturb the receiver.
for (var ctor of integerConstructors) {
    var array = new ctor(2048);
    for (var i = 0; i < array.length; ++i)
        array[i] = coerce(ctor, nextRandom());
    var expectedReceiver = Array.from(array);
    var expected = sortedReference(array);

    var sorted = array.toSorted();
    shouldBe(sorted instanceof ctor, true);
    shouldBe(sorted === array, false);
    shouldBe(sorted.length, array.length);
    for (var i = 0; i < expected.length; ++i) {
        shouldBe(sorted[i], expected[i]);
        shouldBe(array[i], expectedReceiver[i]);
    }
}

// A view over part of a buffer must not write outside its own range. The scatter passes derive write
// cursors from a histogram, so a mismatch between the two escapes the view.
for (var ctor of integerConstructors) {
    var elementCount = 2048;
    var guardBytes = 16 * ctor.BYTES_PER_ELEMENT;
    var dataBytes = elementCount * ctor.BYTES_PER_ELEMENT;
    var buffer = new ArrayBuffer(dataBytes + guardBytes * 2);
    var whole = new Uint8Array(buffer);
    whole.fill(0xab);

    var view = new ctor(buffer, guardBytes, elementCount);
    for (var i = 0; i < elementCount; ++i)
        view[i] = coerce(ctor, nextRandom());
    var expected = sortedReference(view);

    shouldBe(view.sort(), view);
    for (var i = 0; i < elementCount; ++i)
        shouldBe(view[i], expected[i]);
    for (var i = 0; i < guardBytes; ++i) {
        shouldBe(whole[i], 0xab);
        shouldBe(whole[guardBytes + dataBytes + i], 0xab);
    }
}

// Auto-length views over a resizable buffer read their length through a different path.
for (var ctor of integerConstructors) {
    var elementCount = 2048;
    var dataBytes = elementCount * ctor.BYTES_PER_ELEMENT;
    var buffer = new ArrayBuffer(dataBytes, { maxByteLength: dataBytes * 2 });
    var array = new ctor(buffer);
    for (var i = 0; i < array.length; ++i)
        array[i] = coerce(ctor, nextRandom());
    shouldMatchReference(array);

    buffer.resize(dataBytes * 2);
    shouldBe(array.length, elementCount * 2);
    for (var i = 0; i < array.length; ++i)
        array[i] = coerce(ctor, nextRandom());
    shouldMatchReference(array);

    buffer.resize(dataBytes / 4);
    shouldBe(array.length, elementCount / 4);
    for (var i = 0; i < array.length; ++i)
        array[i] = coerce(ctor, nextRandom());
    shouldMatchReference(array);
}

// An array that is sorted apart from one adjacent pair. This pins the block-at-a-time presortedness
// scan, where an inversion straddling a block boundary is the case most likely to be missed. The
// loop's special cases are its opening blocks and its separately handled trailing block, so every
// position in those is covered, plus a few in between where every block is treated alike.
function inversionPositions(length, stride) {
    var positions = [];
    var head = Math.min(length - 1, 4 * stride);
    for (var i = 0; i < head; ++i)
        positions.push(i);
    for (var i = head; i + 1 < length; i += length >> 3)
        positions.push(i);
    for (var i = Math.max(head, length - 1 - 4 * stride); i + 1 < length; ++i)
        positions.push(i);
    return positions;
}

// Comparing the raw storage keeps this cheap enough to sweep, and is exact: every value here is an
// ordinary finite number, so the sorted result must match the ascending base bit for bit. The widest
// view that divides the storage evenly costs the fewest reads.
function storageView(buffer) {
    if (!(buffer.byteLength % 4))
        return new Uint32Array(buffer);
    return new Uint16Array(buffer);
}

function sweepInversions(ctor, base) {
    var length = base.length;
    var array = new ctor(length);
    var expectedStorage = storageView(base.buffer);
    var actualStorage = storageView(array.buffer);

    for (var position of inversionPositions(length, 16 / ctor.BYTES_PER_ELEMENT)) {
        array.set(base);
        // Swapping neighbours leaves this as the only descending pair.
        var lower = array[position];
        array[position] = array[position + 1];
        array[position + 1] = lower;

        array.sort();
        for (var i = 0; i < actualStorage.length; ++i) {
            if (actualStorage[i] !== expectedStorage[i])
                throw new Error('unsorted for inversion at ' + position + ', length ' + length + ', ' + ctor.name);
        }
    }
}

// At or above the radix sort thresholds for 2 and 4 byte elements, so the scan under test runs for
// those. 8 byte elements have a much higher threshold and get their own lengths further down. One
// length is a multiple of every block size and one is not, covering a trailing partial block.
var sweepLengths = [1024, 1031];

for (var ctor of integerConstructors) {
    for (var length of sweepLengths) {
        var base = new ctor(length);
        for (var i = 0; i < length; ++i)
            base[i] = coerce(ctor, i);
        sweepInversions(ctor, base);
    }
}

if (typeof SharedArrayBuffer !== 'undefined') {
    for (var ctor of integerConstructors) {
        var elementCount = 2048;
        var dataBytes = elementCount * ctor.BYTES_PER_ELEMENT;

        var buffer = new SharedArrayBuffer(dataBytes);
        var array = new ctor(buffer);
        for (var i = 0; i < array.length; ++i)
            array[i] = coerce(ctor, nextRandom());
        shouldMatchReference(array);

        var growable = new SharedArrayBuffer(dataBytes, { maxByteLength: dataBytes * 2 });
        var growableArray = new ctor(growable);
        for (var i = 0; i < growableArray.length; ++i)
            growableArray[i] = coerce(ctor, nextRandom());
        shouldMatchReference(growableArray);

        growable.grow(dataBytes * 2);
        shouldBe(growableArray.length, elementCount * 2);
        for (var i = 0; i < growableArray.length; ++i)
            growableArray[i] = coerce(ctor, nextRandom());
        shouldMatchReference(growableArray);
    }
}

// -Infinity < negative finite < -0.0 < +0.0 < positive finite < +Infinity < NaN, and NaN bit patterns
// are canonicalized rather than ordered among themselves.
function compareFloats(a, b) {
    var aIsNaN = Number.isNaN(a);
    var bIsNaN = Number.isNaN(b);
    if (aIsNaN)
        return bIsNaN ? 0 : 1;
    if (bIsNaN)
        return -1;
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    if (a === 0 && b === 0) {
        var aIsNegativeZero = Object.is(a, -0);
        if (aIsNegativeZero === Object.is(b, -0))
            return 0;
        return aIsNegativeZero ? -1 : 1;
    }
    return 0;
}

function shouldMatchFloatReference(array) {
    var expected = Array.from(array).sort(compareFloats);
    shouldBe(array.sort(), array);
    for (var i = 0; i < expected.length; ++i) {
        // NaN !== NaN, so compare NaN-ness rather than value.
        if (Number.isNaN(expected[i])) {
            if (!Number.isNaN(array[i]))
                throw new Error('expected NaN at index ' + i + ', got ' + array[i] + ' in ' + array.constructor.name);
        } else
            shouldBe(array[i], expected[i]);
    }
}

var floatConstructors = [Float16Array, Float32Array, Float64Array];

var floatPatterns = {
    random: function (i, length) { return (nextRandom() - 8388608) / 1024; },
    identical: function (i, length) { return 1.5; },
    ascending: function (i, length) { return i * 0.5; },
    descending: function (i, length) { return (length - i) * 0.5; },
    subnormalAndTiny: function (i, length) { return (i % 2) ? 5e-8 : 6e-8; },
    largeMagnitude: function (i, length) { return (i % 2) ? -1e30 : 1e30; },
    withZeros: function (i, length) { return (i % 3 === 0) ? -0 : ((i % 3 === 1) ? 0 : (nextRandom() / 65536)); },
    withInfinities: function (i, length) { return (i % 4 === 0) ? -Infinity : ((i % 4 === 1) ? Infinity : (nextRandom() / 65536 - 64)); },
    withNaN: function (i, length) { return (i % 5 === 0) ? NaN : (nextRandom() / 65536 - 64); },
    allNaN: function (i, length) { return NaN; },
    negativeNaN: function (i, length) { return (i % 2) ? -NaN : 1; },
};

for (var ctor of floatConstructors) {
    for (var length of lengths) {
        for (var name in floatPatterns) {
            var array = new ctor(length);
            for (var i = 0; i < length; ++i)
                array[i] = floatPatterns[name](i, length);
            shouldMatchFloatReference(array);
        }
    }
}

// -0.0 sorts before +0.0, which === cannot see. This is the case a plain float comparison in the
// presortedness scan would miss, so check the sign of each zero directly.
for (var ctor of floatConstructors) {
    for (var length of [256, 512, 1024, 2048]) {
        var array = new ctor(length);
        var negativeZeroCount = 0;
        for (var i = 0; i < length; ++i) {
            // Start in the wrong order, so the sort has to move the zeros.
            array[i] = (i % 2) ? -0 : 0;
            if (i % 2)
                ++negativeZeroCount;
        }
        array.sort();
        for (var i = 0; i < length; ++i) {
            if (Object.is(array[i], -0) !== (i < negativeZeroCount))
                throw new Error('zero sign wrong at index ' + i + ' of ' + length + ' in ' + ctor.name);
        }
    }
}

// A NaN written through another view over the same buffer keeps whatever bit pattern it was given,
// including a set sign bit. It must still sort last, which is what canonicalizing NaN buys.
var rawNaNs = new Map([
    [Float16Array, [Uint16Array, 0xfe01, 0x7e55]],
    [Float32Array, [Uint32Array, 0xffc00001, 0x7fc00123]],
    [Float64Array, [BigUint64Array, 0xfff8000000000001n, 0x7ff8000000000123n]],
]);

for (var ctor of floatConstructors) {
    var length = 1024;
    var array = new ctor(length);
    for (var i = 0; i < length; ++i)
        array[i] = i - 512;

    var recipe = rawNaNs.get(ctor);
    var raw = new recipe[0](array.buffer);
    raw[0] = recipe[1];
    raw[length - 1] = recipe[2];

    var nanCount = 0;
    for (var i = 0; i < length; ++i) {
        if (Number.isNaN(array[i]))
            ++nanCount;
    }
    shouldBe(nanCount, 2);

    array.sort();
    shouldBe(Number.isNaN(array[length - 1]), true);
    shouldBe(Number.isNaN(array[length - 2]), true);
    for (var i = 0; i < length - 2; ++i) {
        if (Number.isNaN(array[i]))
            throw new Error('NaN did not sort last, found at index ' + i + ' in ' + ctor.name);
        if (i && array[i] < array[i - 1])
            throw new Error('unsorted at index ' + i + ' in ' + ctor.name);
    }
}

// A view over part of a buffer must not write outside its own range, for floats too.
for (var ctor of floatConstructors) {
    var elementCount = 2048;
    var guardBytes = 16 * ctor.BYTES_PER_ELEMENT;
    var dataBytes = elementCount * ctor.BYTES_PER_ELEMENT;
    var buffer = new ArrayBuffer(dataBytes + guardBytes * 2);
    var whole = new Uint8Array(buffer);
    whole.fill(0xab);

    var view = new ctor(buffer, guardBytes, elementCount);
    for (var i = 0; i < elementCount; ++i)
        view[i] = (nextRandom() - 8388608) / 1024;
    var expected = Array.from(view).sort(compareFloats);

    shouldBe(view.sort(), view);
    for (var i = 0; i < elementCount; ++i)
        shouldBe(view[i], expected[i]);
    for (var i = 0; i < guardBytes; ++i) {
        shouldBe(whole[i], 0xab);
        shouldBe(whole[guardBytes + dataBytes + i], 0xab);
    }
}

// The same inversion sweep over the float presortedness scan, which additionally has to compute the
// key transform per lane.
for (var ctor of floatConstructors) {
    for (var length of sweepLengths) {
        var base = new ctor(length);
        for (var i = 0; i < length; ++i)
            base[i] = (i - length / 2) * 0.5;
        sweepInversions(ctor, base);
    }
}

if (typeof SharedArrayBuffer !== 'undefined') {
    for (var ctor of floatConstructors) {
        var elementCount = 2048;
        var buffer = new SharedArrayBuffer(elementCount * ctor.BYTES_PER_ELEMENT);
        var array = new ctor(buffer);
        for (var i = 0; i < array.length; ++i)
            array[i] = (i % 7 === 0) ? NaN : (nextRandom() - 8388608) / 1024;
        shouldMatchFloatReference(array);
    }
}

// The radix sort threshold for 8-byte elements sits far above every length used above, so 8-byte
// types need their own cases to reach the radix path at all. Sorting a shuffled permutation of a
// strictly ascending base has to reproduce that base exactly, which verifies the result without a
// reference sort of BigInt values.
var wideConstructors = [BigInt64Array, BigUint64Array, Float64Array];

// Strictly ascending, and spread so that no digit holds a single value: an all-integral double would
// leave its low mantissa bytes zero, and consecutive integers would leave the high bytes constant.
function ascendingWideBase(ctor, length) {
    var base = new ctor(length);
    if (isBigIntArray(ctor)) {
        for (var i = 0; i < length; ++i)
            base[i] = BigInt(i) * 2654435761n + 12345n;
    } else {
        for (var i = 0; i < length; ++i)
            base[i] = i * 3 + 1 + i * 1e-9;
    }
    return base;
}

function shouldRestoreBase(ctor, base, array, description) {
    var expected = storageView(base.buffer);
    var actual = storageView(array.buffer);
    array.sort();
    for (var i = 0; i < actual.length; ++i) {
        if (actual[i] !== expected[i])
            throw new Error(description + ' at storage index ' + i + ', length ' + base.length + ', ' + ctor.name);
    }
}

var wideLengths = [8192, 8193, 8199];

for (var ctor of wideConstructors) {
    for (var length of wideLengths) {
        var base = ascendingWideBase(ctor, length);
        var array = new ctor(length);
        array.set(base);
        // Deterministic Fisher-Yates, so the sort sees a thoroughly unsorted permutation.
        for (var i = length - 1; i > 0; --i) {
            var j = nextRandom() % (i + 1);
            var swapped = array[i];
            array[i] = array[j];
            array[j] = swapped;
        }
        shouldRestoreBase(ctor, base, array, 'shuffled permutation not restored');
    }
}

// Reversed input reaches the radix path with every digit maximally out of order.
for (var ctor of wideConstructors) {
    var base = ascendingWideBase(ctor, 8192);
    var array = new ctor(8192);
    for (var i = 0; i < 8192; ++i)
        array[i] = base[8191 - i];
    shouldRestoreBase(ctor, base, array, 'reversed input not sorted');
}

// Two distinct values over a long array: few enough that the sort declines radix sort and hands off,
// so this covers the handoff rather than the radix passes.
for (var ctor of wideConstructors) {
    var length = 8192;
    var low = coerce(ctor, 7);
    var high = coerce(ctor, 30000);
    var array = new ctor(length);
    var highCount = 0;
    for (var i = 0; i < length; ++i) {
        var useHigh = !!(nextRandom() & 1);
        array[i] = useHigh ? high : low;
        if (useHigh)
            ++highCount;
    }
    array.sort();
    for (var i = 0; i < length; ++i) {
        var expected = (i < length - highCount) ? low : high;
        if (array[i] !== expected)
            throw new Error('two value sort wrong at ' + i + ' of ' + length + ', ' + ctor.name);
    }
}

// The inversion sweep again, at a length that reaches the radix path for 8-byte elements.
for (var ctor of wideConstructors)
    sweepInversions(ctor, ascendingWideBase(ctor, 8192));
