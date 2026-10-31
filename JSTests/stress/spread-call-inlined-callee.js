function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function callee(a, b, c) {
    return arguments.length + ":" + String(a) + "," + String(b) + "," + String(c);
}

function last() {
    return arguments[arguments.length - 1];
}

class Point {
    constructor(x, y, z) {
        this.result = arguments.length + ":" + String(x) + "," + String(y) + "," + String(z);
    }
}

function callArray(array) { return callee(...array); }
noInline(callArray);
function callLast(array) { return last(...array); }
noInline(callLast);
function callIterable(iterable) { return callee(...iterable); }
noInline(callIterable);
function construct(iterable) { return new Point(...iterable); }
noInline(construct);
function callConstantDoubles() { return callee(...[1.5, 2.5, 3.5]); }
noInline(callConstantDoubles);
function constructConstantDoubles() { return new Point(...[1.5, 2.5, 3.5]); }
noInline(constructConstantDoubles);

function* generate() {
    yield 1;
    yield 2;
}
function createArguments() { return arguments; }

const int32s = [1, 2, 3];
const doubles = [1.5, -0, 2.5];
const objects = ["a", {}, null];
const empty = [];
const five = [1, 2, 3, 4, 5];
const fiveDoubles = [1.5, 2.5, 3.5, 4.5, -0];
const withHole = [1, , 3];
const doublesWithHole = [1.5, , 2.5];
const set = new Set(["a", "b", "c"]);
const emptySet = new Set;
const map = new Map([[1, 2]]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(callArray(int32s), "3:1,2,3");
    shouldBe(callArray(doubles), "3:1.5,0,2.5");
    shouldBe(callArray(objects), "3:a,[object Object],null");
    shouldBe(callArray(empty), "0:undefined,undefined,undefined");
    shouldBe(callArray(five), "5:1,2,3");
    shouldBe(callArray(withHole), "3:1,undefined,3");
    shouldBe(callArray(doublesWithHole), "3:1.5,undefined,2.5");

    shouldBe(callLast(int32s), 3);
    shouldBe(callLast(five), 5);
    shouldBe(Object.is(callLast(fiveDoubles), -0), true);
    shouldBe(callLast(empty), undefined);

    shouldBe(callIterable(set), "3:a,b,c");
    shouldBe(callIterable(emptySet), "0:undefined,undefined,undefined");
    shouldBe(callIterable("xy"), "2:x,y,undefined");
    shouldBe(callIterable(generate()), "2:1,2,undefined");
    shouldBe(callIterable(map.keys()), "1:1,undefined,undefined");
    shouldBe(callIterable(createArguments(1, 2, 3)), "3:1,2,3");
    shouldBe(callIterable(int32s), "3:1,2,3");

    shouldBe(construct(int32s).result, "3:1,2,3");
    shouldBe(construct(doubles).result, "3:1.5,0,2.5");
    shouldBe(construct(empty).result, "0:undefined,undefined,undefined");
    shouldBe(construct(set).result, "3:a,b,c");
    shouldBe(construct("xy").result, "2:x,y,undefined");

    shouldBe(callConstantDoubles(), "3:1.5,2.5,3.5");
    shouldBe(constructConstantDoubles().result, "3:1.5,2.5,3.5");
}

Array.prototype[1] = "fromPrototype";
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(callArray(int32s), "3:1,2,3");
    shouldBe(callArray(withHole), "3:1,fromPrototype,3");
    shouldBe(construct(doublesWithHole).result, "3:1.5,fromPrototype,2.5");
}
