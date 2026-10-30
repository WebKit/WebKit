//@ skip if $memoryLimited
//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function shouldThrowRangeError(func) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error("Expected RangeError, got " + error);
}

function callee() { return arguments.length; }

function materialized(a) { return callee(0, ...a, ...a); }
noInline(materialized);

function materializedAndCaught(x, a) {
    var y = x + 1;
    try {
        return callee(x, ...a, ...a);
    } catch (e) {
        return (e instanceof RangeError) ? x + y : e;
    }
}
noInline(materializedAndCaught);

function forwardedAndMaterialized(a, ...rest) { return callee(...rest, ...a, ...a); }
noInline(forwardedAndMaterialized);

function constantAndMaterialized(a) { return callee(...[1, 2], ...a, ...a); }
noInline(constantAndMaterialized);

function forwarded(...rest) { return callee(0, ...rest, ...rest, ...rest, ...rest, ...rest, ...rest, ...rest, ...rest); }
noInline(forwarded);

function makeClosure(x) {
    return function closure(a) { return callee(x, ...a, ...a); };
}
var closures = [makeClosure(1), makeClosure(2)];
function callClosure(f, a) { return f(a); }
noInline(callClosure);

var small = [1, 2, 3];
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(materialized(small), 7);
    shouldBe(materializedAndCaught(i, small), 7);
    shouldBe(forwardedAndMaterialized(small, 1, 2), 8);
    shouldBe(constantAndMaterialized(small), 8);
    shouldBe(forwarded(1, 2), 17);
    shouldBe(callClosure(closures[i & 1], small), 7);
}

var medium = new Array(200).fill(0);
shouldBe(materialized(medium), 401);
shouldBe(materializedAndCaught(20, medium), 401);
shouldBe(forwardedAndMaterialized(medium, ...medium), 600);
shouldBe(constantAndMaterialized(medium), 402);
shouldBe(forwarded(...medium), 1601);
shouldBe(callClosure(closures[0], medium), 401);

var big = new Array(0x80000).fill(0);
shouldThrowRangeError(() => materialized(big));
shouldBe(materializedAndCaught(20, big), 41);
shouldThrowRangeError(() => forwardedAndMaterialized(big, 1, 2));
shouldThrowRangeError(() => constantAndMaterialized(big));
shouldThrowRangeError(() => forwarded.apply(null, new Array(0x20000).fill(0)));
shouldThrowRangeError(() => callClosure(closures[0], big));

shouldBe(materialized(small), 7);
shouldBe(materializedAndCaught(20, small), 7);
shouldBe(forwardedAndMaterialized(small, 1, 2), 8);
shouldBe(constantAndMaterialized(small), 8);
shouldBe(forwarded(1, 2), 17);
shouldBe(callClosure(closures[0], small), 7);
