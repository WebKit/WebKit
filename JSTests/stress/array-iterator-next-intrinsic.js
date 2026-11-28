function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function collect(iterator) {
    var result = [];
    var step;
    while (!(step = iterator.next()).done)
        result.push(step.value);
    shouldBe(step.value, undefined);
    shouldBe(Object.keys(step).join(','), 'value,done');
    // An exhausted iterator stays exhausted.
    step = iterator.next();
    shouldBe(step.done, true);
    shouldBe(step.value, undefined);
    return result;
}
noInline(collect);

function sumValues(iterable) {
    var result = 0;
    for (var value of iterable)
        result += value;
    return result;
}
noInline(sumValues);

function sumEntries(iterable) {
    var result = 0;
    for (var [index, value] of iterable.entries())
        result += index * 1000 + value;
    return result;
}
noInline(sumEntries);

function sumKeys(iterable) {
    var result = 0;
    for (var index of iterable.keys())
        result += index;
    return result;
}
noInline(sumKeys);

var typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];
if (typeof Float16Array !== "undefined")
    typedArrays.push(Float16Array);

for (var constructor of typedArrays) {
    var array = new constructor([1, 2, 3, 4]);
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(collect(array.values()).join(','), '1,2,3,4');
        shouldBe(collect(array.keys()).join(','), '0,1,2,3');
        shouldBe(JSON.stringify(collect(array.entries())), '[[0,1],[1,2],[2,3],[3,4]]');
        shouldBe(sumValues(array), 10);
        shouldBe(sumEntries(array), 6010);
        shouldBe(sumKeys(array), 6);
    }
}

var bigArray = new BigInt64Array([1n, 2n, 3n]);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(collect(bigArray.values()).join(','), '1,2,3');

// Plain arrays reached through next(): holes read through the prototype chain.
var holey = [1, , 3];
Array.prototype[1] = 42;
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(collect([1, 2, 3].values()).join(','), '1,2,3');
    shouldBe(collect([1.5, 2.5].values()).join(','), '1.5,2.5');
    shouldBe(collect(['a', {}].keys()).join(','), '0,1');
    shouldBe(collect(holey.values()).join(','), '1,42,3');
}
delete Array.prototype[1];
shouldBe(collect(holey.values()).join(','), '1,,3');

// Array-likes.
var arrayLike = { length: 3, 0: 'a', 1: 'b', 2: 'c' };
var lengthReads = 0;
var arrayLikeWithGetter = { get length() { ++lengthReads; return 2; }, 0: 'x', 1: 'y' };
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(collect(Array.prototype.values.call(arrayLike)).join(','), 'a,b,c');
    lengthReads = 0;
    shouldBe(collect(Array.prototype.values.call(arrayLikeWithGetter)).join(','), 'x,y');
    // Read once per next() until exhausted, never after.
    shouldBe(lengthReads, 3);
}

// The iterated length is re-read on every step.
function growWhileIterating(array) {
    var iterator = array.values();
    var result = [];
    var step;
    while (!(step = iterator.next()).done) {
        result.push(step.value);
        if (array.length < 5)
            array.push(array.length);
    }
    return result.join(',');
}
noInline(growWhileIterating);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(growWhileIterating([0]), '0,1,2,3,4');

// Mixed kinds and iterated object types through one call site.
function makeMixed(i) {
    switch (i % 5) {
    case 0: return [[1, 2].values(), '1,2'];
    case 1: return [new Int32Array([3, 4]).values(), '3,4'];
    case 2: return [new Float64Array([0.5]).entries(), '0,0.5'];
    case 3: return [['p', 'q'].keys(), '0,1'];
    default: return [Array.prototype.values.call({ length: 1, 0: 'z' }), 'z'];
    }
}
for (var i = 0; i < testLoopCount; ++i) {
    var [iterator, expected] = makeMixed(i);
    shouldBe(collect(iterator).join(','), expected);
}

// Brand check.
var next = [].values().next;
function callNextWithBadThis(thisValue) {
    return next.call(thisValue);
}
noInline(callNextWithBadThis);
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(callNextWithBadThis([].values()).done, true);
    if (i % 100 === 0) {
        for (var bad of [undefined, null, 1, 'string', {}, new Map().entries(), new Set().values(), ''[Symbol.iterator]()])
            shouldThrow(() => callNextWithBadThis(bad), 'TypeError: %ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance');
    }
}
