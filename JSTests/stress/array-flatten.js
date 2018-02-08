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

shouldBe([].flatten.length, 0);
shouldBe([].flatten.name, `flatten`);

shouldBeArray([].flatten(), []);
shouldBeArray([0, 1, 2, 3, , 4].flatten(), [0, 1, 2, 3, 4]);
shouldBeArray([,,,,,].flatten(), []);

shouldBeArray([].flatten(0), []);
shouldBeArray([0, 1, 2, 3, , 4].flatten(0), [0, 1, 2, 3, 4]);
shouldBeArray([,,,,,].flatten(0), []);

shouldBeArray([].flatten(-1), []);
shouldBeArray([0, 1, 2, 3, , 4].flatten(-1), [0, 1, 2, 3, 4]);
shouldBeArray([,,,,,].flatten(-1), []);

shouldBeArray([[],[]].flatten(), []);
shouldBeArray([[0],[1]].flatten(), [0,1]);
shouldBeArray([[0],[],1].flatten(), [0,1]);
shouldBeArray([[0],[[]],1].flatten(), [0,[],1]);
shouldBeArray([[0],[[]],1].flatten(1), [0,[],1]);
shouldBeArray([[0],[[]],1].flatten(2), [0,1]);

shouldBeArray([[],[]].flatten(0), [[],[]]);
shouldBeArray([[0],[1]].flatten(0), [[0],[1]]);
shouldBeArray([[0],[],1].flatten(0), [[0],[],1]);
shouldBeArray([[0],[[]],1].flatten(0), [[0],[[]],1]);

shouldBeArray([[[[[[[[[[[[[[[[[[[[[42]]]]]]]]]]]]]]]]]]]]].flatten(Infinity), [42]);

var array = [];
shouldBe(array.flatten() !== array, true);

class DerivedArray extends Array { }
shouldBe((new DerivedArray).flatten() instanceof DerivedArray, true);
var flatten = [].flatten;
var realm = createGlobalObject();
shouldBe(flatten.call({}) instanceof Array, true);
shouldBe(flatten.call(new realm.Array) instanceof Array, true);
var array2 = new realm.Array;
array2.constructor = 0;

shouldThrow(() => {
    flatten.call(array2);
}, `TypeError: 0 is not a constructor`);

var array2 = new realm.Array;
array2.constructor = undefined;
shouldBe(flatten.call(array2) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return null;
    }
};
shouldBe(flatten.call(array2) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return undefined;
    }
};
shouldBe(flatten.call(array2) instanceof Array, true);

var array2 = new realm.Array;
array2.constructor = {
    get [Symbol.species]() {
        return DerivedArray;
    }
};
shouldBe(flatten.call(array2) instanceof DerivedArray, true);
