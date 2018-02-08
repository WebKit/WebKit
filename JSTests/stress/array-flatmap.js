function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            if (Array.isArray(expected[i])) {
                shouldBe(Array.isArray(actual[i]), true);
                shouldBeArray(actual[i], expected[i]);
            } else
                shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
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

shouldThrow(() => {
    [].flatMap();
}, `TypeError: Array.prototype.flatMap callback must be a function`);

var array = [42];
shouldBeArray(array.flatMap(function (v) {
    "use strict";
    shouldBe(v, 42);
    return this;
}, `Cocoa`), [`Cocoa`]);

shouldBeArray([].flatMap((v) => v), []);
shouldBeArray([42].flatMap((v) => v), [42]);
shouldBeArray([42].flatMap((v) => [v]), [42]);
shouldBeArray([42].flatMap((v) => [[v]]), [[42]]);
shouldBeArray([42].flatMap((v) => [v, v, v]), [42,42,42]);
shouldBeArray([42,[43],44].flatMap((v) => [v, v]), [42,42,[43],[43],44,44]);
shouldBeArray([,,,,,,].flatMap((v) => [v, v]), []);
shouldBeArray([42,43,44].flatMap((v) => []), []);
shouldBeArray([42,[43],44].flatMap((v) => v), [42,43,44]);

class DerivedArray extends Array { }
shouldBe((new DerivedArray).flatMap(() => {}) instanceof DerivedArray, true);
var flatMap = [].flatMap;
var realm = createGlobalObject();
shouldBe(flatMap.call({}, () => {}) instanceof Array, true);
shouldBe(flatMap.call(new realm.Array, () => {}) instanceof Array, true);
var array2 = new realm.Array;
array2.constructor = 0;

shouldThrow(() => {
    flatMap.call(array2, () => {});
}, `TypeError: 0 is not a constructor`);

var array2 = new realm.Array;
array2.constructor = undefined;
shouldBe(flatMap.call(array2, () => {}) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return null;
    }
};
shouldBe(flatMap.call(array2, () => {}) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return undefined;
    }
};
shouldBe(flatMap.call(array2, () => {}) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return DerivedArray;
    }
};
shouldBe(flatMap.call(array2, () => {}) instanceof DerivedArray, true);
