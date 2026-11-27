//@ defaultRun; runDefault("--useDFGJIT=false")
function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + String(actual) + " expected " + String(expected));
}

function add(a, b) { return a + b; }
function sub(a, b) { return a - b; }
function mul(a, b) { return a * b; }
noInline(add);
noInline(sub);
noInline(mul);

function untyped(a, b, isNumber) {
    if (isNumber)
        return [a + b, a - b, a * b];
    return a;
}
noInline(untyped);

var negativeNaN = -NaN;
var infinityMinusInfinity = Infinity - Infinity;
var nans = [NaN, negativeNaN, infinityMinusInfinity, 0 / 0];

for (var i = 0; i < testLoopCount / 10; ++i) {
    shouldBe(add(1.5, 2.25), 3.75);
    shouldBe(sub(1.5, 2.25), -0.75);
    shouldBe(mul(1.5, 2.25), 3.375);
    untyped({ }, { }, false);
    untyped("a", "b", false);
    var result = untyped(1.5, 2.25, true);
    shouldBe(result[0], 3.75);
    shouldBe(result[1], -0.75);
    shouldBe(result[2], 3.375);
}

for (var i = 0; i < testLoopCount / 10; ++i) {
    var nan = nans[i % nans.length];
    shouldBe(add(nan, 2.5), NaN);
    shouldBe(add(2.5, nan), NaN);
    shouldBe(sub(nan, 2.5), NaN);
    shouldBe(sub(2.5, nan), NaN);
    shouldBe(mul(nan, 2.5), NaN);
    shouldBe(mul(2.5, nan), NaN);
    shouldBe(add(Infinity, -Infinity), NaN);
    shouldBe(sub(Infinity, Infinity), NaN);
    shouldBe(mul(Infinity, 0.0), NaN);
    shouldBe(add(-0.0, -0.0), -0);
    shouldBe(sub(-0.0, 0.0), -0);
    shouldBe(mul(-0.5, 0.0), -0);
    shouldBe(add(1.5, 2.25), 3.75);
    shouldBe(sub(1.5, 2.25), -0.75);
    shouldBe(mul(1.5, 2.25), 3.375);

    var result = untyped(nan, 2.5, true);
    shouldBe(result[0], NaN);
    shouldBe(result[1], NaN);
    shouldBe(result[2], NaN);
    result = untyped(1.5, 2.25, true);
    shouldBe(result[0], 3.75);
    shouldBe(result[1], -0.75);
    shouldBe(result[2], 3.375);
}
