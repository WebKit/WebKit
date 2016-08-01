function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
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

shouldBe([].copyWithin.name, 'copyWithin');
shouldBe([].copyWithin.length, 2);
shouldBe(Array.prototype.hasOwnProperty('copyWithin'), true);
shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(Array.prototype, 'copyWithin')), '{"writable":true,"enumerable":false,"configurable":true}');

// 0 arguments. (it is equivalent to copyWithin(0))
shouldBeArray([1, 2, 3, 4, 5].copyWithin(), [1, 2, 3, 4, 5]);
shouldBeArray([].copyWithin(), []);

// 1 arguments.
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-4), [1, 1, 2, 3, 4]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-2), [1, 2, 3, 1, 2]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-1), [1, 2, 3, 4, 1]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1), [1, 1, 2, 3, 4]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(2), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(3), [1, 2, 3, 1, 2]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(4), [1, 2, 3, 4, 1]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(6), [1, 2, 3, 4, 5]);

// 2 arguments.
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 0), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1), [2, 3, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 2), [3, 4, 5, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 3), [4, 5, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 4), [5, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 6), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1, 3), [1, 4, 5, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(2, 3), [1, 2, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(3, 3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(4, 3), [1, 2, 3, 4, 4]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(5, 3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(6, 3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-1, -2), [1, 2, 3, 4, 4]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-1, -3), [1, 2, 3, 4, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -3), [3, 4, 5, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -8), [1, 2, 3, 4, 5]);

// 3 arguments.
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 3, 4), [4, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 0, 5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 2), [2, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 3), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 4), [2, 3, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 5), [2, 3, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 1, 6), [2, 3, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(2, 1, 3), [1, 2, 2, 3, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(6, 1, 3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -0, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 0, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -2, -1), [4, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -3, -1), [3, 4, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -4, -1), [2, 3, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -5, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -6, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, 0, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -3, -2), [3, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -4, -2), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -5, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -6, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -0, -1), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, 0, -1), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -1), [1, 2, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -3, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -4, -1), [1, 2, 2, 3, 4]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -5, -1), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -6, -1), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, 0, -2), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -3, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -4, -2), [1, 2, 2, 3, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -5, -2), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -6, -2), [1, 2, 1, 2, 3]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -4), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(0, -1, -6), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -4), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-3, -2, -5), [1, 2, 3, 4, 5]);

shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -0, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 0, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 0, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 0, 5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 2), [2, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 3), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 4), [2, 3, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 5), [2, 3, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 6), [2, 3, 4, 5, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 3), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 1, 3), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, 3, 4), [4, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -1), [4, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -3, -1), [3, 4, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -4, -1), [2, 3, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -5, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -6, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -3, -2), [3, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -4, -2), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -5, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -6, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -1), [4, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -3, -1), [3, 4, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -4, -1), [2, 3, 4, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -5, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -6, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -3, -2), [3, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -4, -2), [2, 3, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -5, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -6, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -1), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -4), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -5), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -1, -6), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -2), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -3), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -4), [1, 2, 3, 4, 5]);
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-6, -2, -5), [1, 2, 3, 4, 5]);

shouldBeArray([1, 2, 3, 4, 5].copyWithin(NaN, 1), [1, 2, 3, 4, 5].copyWithin(0, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(NaN, NaN, 1), [1, 2, 3, 4, 5].copyWithin(0, 0, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(NaN, NaN, NaN), [1, 2, 3, 4, 5].copyWithin(0, 0, 0));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(Infinity, 0, 1), [1, 2, 3, 4, 5].copyWithin(5, 0, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(Infinity, Infinity, 1), [1, 2, 3, 4, 5].copyWithin(5, 5, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1, Infinity, 1), [1, 2, 3, 4, 5].copyWithin(1, 5, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1, -Infinity, 1), [1, 2, 3, 4, 5].copyWithin(1, 0, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(-Infinity, 0, 1), [1, 2, 3, 4, 5].copyWithin(0, 0, 1));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1, 0, Infinity), [1, 2, 3, 4, 5].copyWithin(1, 0, 5));
shouldBeArray([1, 2, 3, 4, 5].copyWithin(1, 0, -Infinity), [1, 2, 3, 4, 5].copyWithin(1, 0, 0));

var copyWithin = Array.prototype.copyWithin;

function arrayToObject(array) {
    var object = {};
    for (var [key, value] of array.entries()) {
        object[key] = value;
    }
    object.length = array.length;
    return object;
}
var object = arrayToObject([1, 2, 3, 4, 5]);
object.length = -Infinity;
shouldBeArray(copyWithin.call(object, 1), { length: -Infinity });
shouldBe(JSON.stringify(copyWithin.call(object, 1)), '{"0":1,"1":2,"2":3,"3":4,"4":5,"length":null}');

var object = arrayToObject([1, 2, 3, 4, 5]);
object.length = -Infinity;
shouldBe(JSON.stringify(copyWithin.call(object, 1)), '{"0":1,"1":2,"2":3,"3":4,"4":5,"length":null}');

var array = [1, 2, 3, 4, 5];
var same = array.copyWithin(0, 3);
shouldBeArray(same, [4, 5, 3, 4, 5]);
shouldBeArray(array, [4, 5, 3, 4, 5]);
shouldBe(same, array);

shouldBe(JSON.stringify([].copyWithin.call({length: 5, 3: 1}, 0, 3)), '{"0":1,"3":1,"length":5}');
shouldBe(JSON.stringify([].copyWithin.call(new Int32Array([1, 2, 3, 4, 5]), 0, 3, 4)), '{"0":4,"1":2,"2":3,"3":4,"4":5}');

shouldBe(JSON.stringify([].copyWithin.call({length: 5.5, 3: 1}, 0, 3)), '{"0":1,"3":1,"length":5.5}');
shouldBe(JSON.stringify([].copyWithin.call({length: 5.5, 3: 1}, 0.1, 3)), '{"0":1,"3":1,"length":5.5}');
shouldBe(JSON.stringify([].copyWithin.call({length: 5.5, 3: 1}, 0.1, 3.3)), '{"0":1,"3":1,"length":5.5}');
shouldBe(JSON.stringify([].copyWithin.call({length: 5.5, 3: 1}, 0.1, 3.8)), '{"0":1,"3":1,"length":5.5}');
shouldBe(JSON.stringify([].copyWithin.call({length: 5.5, 3: 1}, 0.1, 4.1)), '{"3":1,"length":5.5}');

var object = arrayToObject([1, 2, 3, 4, 5]);
delete object[2];
delete object[3];
delete object[4];
var result = copyWithin.call(object, 0, 3, 5);
shouldBe(JSON.stringify(result), '{"length":5}');

// 'copyWithin' in Array's @unscopables
(function () {
    var array = [];
    var copyWithin = 42;
    var forEach = 42;
    with (array) {
        shouldBe(copyWithin, 42);
        shouldBe(forEach, [].forEach);
    }
}());

shouldThrow(function () {
    Array.prototype.copyWithin.call(undefined);
}, 'TypeError: Array.copyWithin requires that |this| not be null or undefined');

shouldThrow(function () {
    Array.prototype.copyWithin.call(null);
}, 'TypeError: Array.copyWithin requires that |this| not be null or undefined');


function valueOf(code) {
    return {
        valueOf() {
            throw new Error(code);
        }
    };
}

shouldThrow(function () {
    var object = arrayToObject([1, 2, 3, 4, 5]);
    object.length = valueOf(0);
    copyWithin.call(object, valueOf(1), valueOf(2), valueOf(3));
}, 'Error: 0');

shouldThrow(function () {
    var object = arrayToObject([1, 2, 3, 4, 5]);
    copyWithin.call(object, valueOf(1), valueOf(2), valueOf(3));
}, 'Error: 1');

shouldThrow(function () {
    var object = arrayToObject([1, 2, 3, 4, 5]);
    copyWithin.call(object, 0, valueOf(2), valueOf(3));
}, 'Error: 2');

shouldThrow(function () {
    var object = arrayToObject([1, 2, 3, 4, 5]);
    copyWithin.call(object, 0, 1, valueOf(3));
}, 'Error: 3');

shouldThrow(function () {
    var object = arrayToObject([1, 2, 3, 4, 5]);
    Object.freeze(object);
    copyWithin.call(object, 0, 1, 2);
}, 'TypeError: Attempted to assign to readonly property.');
