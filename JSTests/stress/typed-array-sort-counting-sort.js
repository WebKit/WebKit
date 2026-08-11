function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

var seed = 1;
function nextRandom() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed |= 0;
    return seed >>> 24;
}

function sortedReference(array) {
    return Array.from(array).sort(function (a, b) { return a - b; });
}

function shouldMatchReference(array) {
    var expected = sortedReference(array);
    shouldBe(array.sort(), array);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(array[i], expected[i]);
}

var constructors = [Int8Array, Uint8Array, Uint8ClampedArray];

// Straddle the length at which the engine switches to counting sort, from well below to well above.
var lengths = [0, 1, 2, 8, 64, 100, 191, 192, 193, 255, 256, 257, 1000];

var patterns = {
    random: function (i, length) { return nextRandom(); },
    identical: function (i, length) { return 42; },
    ascending: function (i, length) { return i; },
    descending: function (i, length) { return length - i; },
    extremes: function (i, length) { return (i % 3 === 0) ? -128 : ((i % 3 === 1) ? 255 : 0); },
    twoValues: function (i, length) { return (i % 2) ? 200 : 7; },
};

for (var ctor of constructors) {
    for (var length of lengths) {
        for (var name in patterns) {
            var array = new ctor(length);
            for (var i = 0; i < length; ++i)
                array[i] = patterns[name](i, length);
            shouldMatchReference(array);
        }
    }
}

// toSorted takes the same no-comparator path, and must not disturb the receiver.
for (var ctor of constructors) {
    var array = new ctor(300);
    for (var i = 0; i < array.length; ++i)
        array[i] = nextRandom();
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

// A view over part of a buffer must not write outside its own range.
for (var ctor of constructors) {
    var elementCount = 300;
    var guard = 16;
    var buffer = new ArrayBuffer(elementCount + guard * 2);
    var whole = new Uint8Array(buffer);
    whole.fill(0xab);

    var view = new ctor(buffer, guard, elementCount);
    for (var i = 0; i < elementCount; ++i)
        view[i] = nextRandom();
    var expected = sortedReference(view);

    shouldBe(view.sort(), view);
    for (var i = 0; i < elementCount; ++i)
        shouldBe(view[i], expected[i]);
    for (var i = 0; i < guard; ++i) {
        shouldBe(whole[i], 0xab);
        shouldBe(whole[guard + elementCount + i], 0xab);
    }
}

// Auto-length views over a resizable buffer read their length through a different path.
for (var ctor of constructors) {
    var buffer = new ArrayBuffer(300, { maxByteLength: 600 });
    var array = new ctor(buffer);
    for (var i = 0; i < array.length; ++i)
        array[i] = nextRandom();
    shouldMatchReference(array);

    buffer.resize(600);
    shouldBe(array.length, 600);
    for (var i = 0; i < array.length; ++i)
        array[i] = nextRandom();
    shouldMatchReference(array);

    buffer.resize(70);
    shouldBe(array.length, 70);
    for (var i = 0; i < array.length; ++i)
        array[i] = nextRandom();
    shouldMatchReference(array);
}

// An array that is sorted apart from one adjacent pair, with that pair placed at every position in
// turn. This pins the block-at-a-time presortedness scan: a boundary it fails to cover would leave
// the array unsorted for exactly one of these positions.
for (var ctor of constructors) {
    var base = (ctor === Int8Array) ? -128 : 0;
    for (var length of [128, 129, 130, 143, 144, 145, 159, 160, 161, 200, 256]) {
        var expected = [];
        for (var i = 0; i < length; ++i)
            expected.push(base + i);

        for (var position = 0; position + 1 < length; ++position) {
            var array = new ctor(length);
            for (var i = 0; i < length; ++i)
                array[i] = base + i;
            // Swapping neighbours leaves this as the only descending pair.
            array[position] = base + position + 1;
            array[position + 1] = base + position;

            array.sort();
            for (var i = 0; i < length; ++i) {
                if (array[i] !== expected[i])
                    throw new Error('unsorted at index ' + i + ' for inversion at ' + position + ', length ' + length + ', ' + ctor.name);
            }
        }
    }
}

if (typeof SharedArrayBuffer !== 'undefined') {
    for (var ctor of constructors) {
        var buffer = new SharedArrayBuffer(300);
        var array = new ctor(buffer);
        for (var i = 0; i < array.length; ++i)
            array[i] = nextRandom();
        shouldMatchReference(array);
    }

    for (var ctor of constructors) {
        var buffer = new SharedArrayBuffer(300, { maxByteLength: 600 });
        var array = new ctor(buffer);
        for (var i = 0; i < array.length; ++i)
            array[i] = nextRandom();
        shouldMatchReference(array);

        buffer.grow(600);
        shouldBe(array.length, 600);
        for (var i = 0; i < array.length; ++i)
            array[i] = nextRandom();
        shouldMatchReference(array);
    }
}
