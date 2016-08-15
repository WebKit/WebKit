function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function iterator(array) {
    var nextCount = 0;
    var returnCount = 0;
    var original =  array.values();
    return {
        [Symbol.iterator]() {
            return this;
        },

        next() {
            ++nextCount;
            return original.next();
        },

        return() {
            ++returnCount;
            return { done: true };
        },

        reportNext() {
            return nextCount;
        },

        reportReturn() {
            return returnCount;
        }
    };
};

(function () {
    var iter = iterator([1, 2, 3]);
    var [] = iter;
    shouldBe(iter.reportNext(), 0);
    shouldBe(iter.reportReturn(), 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,] = iter;
    shouldBe(iter.reportNext(), 1);
    shouldBe(iter.reportReturn(), 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,,] = iter;
    shouldBe(iter.reportNext(), 2);
    shouldBe(iter.reportReturn(), 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,,,] = iter;
    shouldBe(iter.reportNext(), 3);
    shouldBe(iter.reportReturn(), 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,,,,] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,,,,,] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [,a,] = iter;
    shouldBe(iter.reportNext(), 2);
    shouldBe(iter.reportReturn(), 1);
    shouldBe(a, 2);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [a,] = iter;
    shouldBe(iter.reportNext(), 1);
    shouldBe(iter.reportReturn(), 1);
    shouldBe(a, 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [a,,] = iter;
    shouldBe(iter.reportNext(), 2);
    shouldBe(iter.reportReturn(), 1);
    shouldBe(a, 1);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [a,b = 42,] = iter;
    shouldBe(iter.reportNext(), 2);
    shouldBe(iter.reportReturn(), 1);
    shouldBe(a, 1);
    shouldBe(b, 2);
}());

(function () {
    var {} = { Cocoa: 15, Cappuccino: 13 };
}());

(function () {
    var {Cocoa,} = { Cocoa: 15, Cappuccino: 13 };
    shouldBe(Cocoa, 15);
}());

(function () {
    var {Cocoa = 'Cocoa',} = { Cocoa: 15, Cappuccino: 13 };
    shouldBe(Cocoa, 15);
}());

(function () {
    var {Cocoa, Kilimanjaro = 'Coffee'} = { Cocoa: 15, Cappuccino: 13 };
    shouldBe(Cocoa, 15);
    shouldBe(Kilimanjaro, 'Coffee');
}());

(function () {
    var {Cocoa, Kilimanjaro = 'Coffee'} = {};
    shouldBe(Cocoa, undefined);
    shouldBe(Kilimanjaro, 'Coffee');
}());

(function () {
    var {Cocoa, Kilimanjaro = 'Coffee',} = { Cocoa: 15, Cappuccino: 13 };
    shouldBe(Cocoa, 15);
    shouldBe(Kilimanjaro, 'Coffee');
}());

function testSyntaxError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected syntax error not thrown");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

testSyntaxError(String.raw`var {,} = {Cocoa: 15}`, String.raw`SyntaxError: Unexpected token ','. Expected a property name.`);
testSyntaxError(String.raw`var {,} = {}`, String.raw`SyntaxError: Unexpected token ','. Expected a property name.`);
