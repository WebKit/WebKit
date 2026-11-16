function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

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

(function () {
    var [a, b, ...c] = "Cocoa";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`["c","o","a"]`);
}());

(function () {
    var [a, b, ...c] = "Co";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`[]`);
}());

(function () {
    var [a, b, ...c] = "C";
    shouldBe(a, 'C');
    shouldBe(b, undefined);
    shouldBe(JSON.stringify(c), String.raw`[]`);
}());

(function () {
    var a, b, c;
    [a, b, ...c] = "Cocoa";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`["c","o","a"]`);
}());

(function () {
    var a, b, c;
    [a, b, ...c] = "Co";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`[]`);
}());

(function () {
    var a, b, c;
    [a, b, ...c] = "C";
    shouldBe(a, 'C');
    shouldBe(b, undefined);
    shouldBe(JSON.stringify(c), String.raw`[]`);
}());

(function ([a, b, ...c]) {
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`["c","o","a"]`);
}("Cocoa"));

(function ([a, b, ...c]) {
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`[]`);
}("Co"));

(function ([a, b, ...c]) {
    shouldBe(a, 'C');
    shouldBe(b, undefined);
    shouldBe(JSON.stringify(c), String.raw`[]`);
}("C"));

testSyntaxError(String.raw`var [a, ...b, c] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`var [a, ...b,] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`var [a, ...b,,] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`var [a, ...b = 20] = 20`, String.raw`SyntaxError: Unexpected token '='. Expected a closing ']' following a rest element destructuring pattern.`);

testSyntaxError(String.raw`(function ([a, ...b,]) { })`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`(function ([a, ...b,,]) { })`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`(function ([a, ...b = 20,,]) { })`, String.raw`SyntaxError: Unexpected token '='. Expected a closing ']' following a rest element destructuring pattern.`);


testSyntaxError(String.raw`[a, ...b, c] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`[a, ...b,] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`[a, ...b,,] = 20`, String.raw`SyntaxError: Unexpected token ','. Expected a closing ']' following a rest element destructuring pattern.`);
testSyntaxError(String.raw`[a, ...b = 20] = 20`, String.raw`SyntaxError: Unexpected token '='. Expected a closing ']' following a rest element destructuring pattern.`);

(function () {
    var a, b, c;
    [a, b, ...[...c]] = "Cocoa";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(JSON.stringify(c), String.raw`["c","o","a"]`);
}());

(function () {
    var a, b, c, d, e, f;
    [a, b, ...{ 0: c, 1: d, 2: e, 3: f }] = "Cocoa";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(c, 'c');
    shouldBe(d, 'o');
    shouldBe(f, undefined);
}());

(function () {
    var a, b, c, d, e;
    [a, b, ...[c, d, ...e]] = "Cocoa";
    shouldBe(a, 'C');
    shouldBe(b, 'o');
    shouldBe(c, 'c');
    shouldBe(d, 'o');
    shouldBe(JSON.stringify(e), String.raw`["a"]`);
}());

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
    var [...a] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
    shouldBe(JSON.stringify(a), String.raw`[1,2,3]`);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [a, b, ...c] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(JSON.stringify(c), String.raw`[3]`);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var [a, b, c, d, ...e] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(c, 3);
    shouldBe(d, undefined);
    shouldBe(JSON.stringify(e), String.raw`[]`);
}());

(function () {
    var iter = iterator([1, 2, 3]);
    var a, b;
    [...[a, b]] = iter;
    shouldBe(iter.reportNext(), 4);
    shouldBe(iter.reportReturn(), 0);
    shouldBe(a, 1);
    shouldBe(b, 2);
}());
