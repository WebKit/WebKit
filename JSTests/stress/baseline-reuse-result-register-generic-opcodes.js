function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function bitAndThenCompare(a, b, k) {
    if (k < (a & b))
        return 1;
    return 0;
}
noInline(bitAndThenCompare);

function thisInstanceOf() {
    return this instanceof Number;
}
noInline(thisInstanceOf);

function makeTDZ() {
    function read() { return x + 1; }
    let result = read;
    let x = 41;
    return result;
}
var readTDZ = makeTDZ();
noInline(readTDZ);

var globalValue = 10;
function readGlobal(a) {
    return globalValue + a;
}
noInline(readGlobal);

function makeCounter() {
    var count = 0;
    return function (a, b) {
        count = a + b;
        return count;
    };
}
var counter = makeCounter();
noInline(counter);

function constantThenAdd(y) {
    var x = 5;
    return x + y;
}
noInline(constantThenAdd);

function getByValThenCompare(array, index, k) {
    return k < array[index];
}
noInline(getByValThenCompare);

function toNumberInPlace(x) {
    x = +x;
    return x * 2;
}
noInline(toNumberInPlace);

var array = [1, 2, 3, 4];
for (var i = 0; i < testLoopCount * 2; ++i) {
    shouldBe(bitAndThenCompare(7, 6, 5), 1);
    shouldBe(thisInstanceOf.call(new Number(1)), true);
    shouldBe(readTDZ(), 42);
    shouldBe(readGlobal(1), globalValue + 1);
    shouldBe(counter(1, 2), 3);
    shouldBe(constantThenAdd(1), 6);
    shouldBe(getByValThenCompare(array, 2, 2), true);
    shouldBe(toNumberInPlace(3), 6);
    if (!(i % 97)) {
        shouldBe(bitAndThenCompare(7, 6, 6.5), 0);
        shouldBe(bitAndThenCompare({ valueOf() { return 7; } }, 6, 5), 1);
        shouldBe(thisInstanceOf.call(1), true);
        shouldBe(counter("a", 2), "a2");
        shouldBe(constantThenAdd("y"), "5y");
        shouldBe(getByValThenCompare(array, 10, 2), false);
        shouldBe(getByValThenCompare("abcd", 1, "a"), true);
        shouldBe(toNumberInPlace("4"), 8);
        shouldBe(toNumberInPlace({ valueOf() { return 5; } }), 10);
        globalThis["extraGlobal" + i] = i;
        globalValue = i;
        shouldBe(readGlobal(1), i + 1);
    }
}
