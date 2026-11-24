function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

class A {
    constructor(value) { this.value = value; }
    run(x) { return this.value + x; }
}

class B {
    constructor(value) { this.other = 1; this.value = value; }
    run(x) { return this.value * x; }
}

class C {
    constructor(value) { this.value = value; }
    run(x) { return this.value - x; }
}

// Two shapes of D share D.prototype.run.
class D {
    constructor(value, extra) {
        if (extra)
            this.extra = extra;
        this.value = value;
    }
    run(x) { return this.value + this.value + x; }
}

let counter = 0;
class E {
    constructor(value) { this.value = value; }
    run(x) {
        let self = this;
        function inner() { return self.value + counter; }
        return inner() + x;
    }
}

function expected(object, x) {
    if (object instanceof A)
        return object.value + x;
    if (object instanceof B)
        return object.value * x;
    if (object instanceof C)
        return object.value - x;
    if (object instanceof D)
        return object.value + object.value + x;
    if (object instanceof E)
        return object.value + counter + x;
    throw new Error("unexpected object");
}

function callWithResult(object, x) {
    return object.run(x);
}
noInline(callWithResult);

let sideEffect = 0;
function callIgnoringResult(object, x) {
    object.run(x);
    sideEffect++;
}
noInline(callIgnoringResult);

let objects = [new A(1), new B(2), new C(3), new D(4), new D(5, true), new E(6)];
for (let i = 0; i < testLoopCount; ++i) {
    let object = objects[i % objects.length];
    shouldBe(callWithResult(object, i), expected(object, i));
    callIgnoringResult(object, i);
}
shouldBe(sideEffect, testLoopCount);

// A structure that the compiled code has not seen takes the exit.
class F {
    constructor(value) { this.value = value; }
    run(x) { return this.value * 100 + x; }
}
let f = new F(7);
shouldBe(callWithResult(f, 3), 703);

// Replacing a method invalidates the code that called it as a constant.
B.prototype.run = function (x) { return this.value * x + 1000; };
for (let i = 0; i < testLoopCount; ++i) {
    let object = objects[i % objects.length];
    let result = callWithResult(object, i);
    if (object instanceof B)
        shouldBe(result, object.value * i + 1000);
    else
        shouldBe(result, expected(object, i));
}

class Thrower {
    run() { throw new Error("thrown"); }
}
let thrower = new Thrower();
let caught = 0;
function callMaybeThrowing(object) {
    try {
        return object.run(1);
    } catch (error) {
        shouldBe(error.message, "thrown");
        caught++;
    }
}
noInline(callMaybeThrowing);
for (let i = 0; i < testLoopCount; ++i) {
    let object = i % 3 ? objects[i % objects.length] : thrower;
    callMaybeThrowing(object);
}
shouldBe(caught, Math.ceil(testLoopCount / 3));

// The call has never returned when the caller is compiled, so its value profile is empty.
class AlwaysThrowsA {
    fail(x) { throw new Error("a" + x); }
}
class AlwaysThrowsB {
    constructor() { this.y = 1; }
    fail(x) { throw new Error("b" + x); }
}
function maybeCallThrowing(object, n) {
    let s = 0;
    for (let i = 0; i < n; ++i)
        s += i;
    if (n === 0)
        return object.fail(s);
    return s;
}
noInline(maybeCallThrowing);
let throwers = [new AlwaysThrowsA, new AlwaysThrowsB];
for (let i = 0; i < 2000; ++i)
    shouldBe(maybeCallThrowing(throwers[i & 1], 10), 45);
for (let i = 0; i < 200; ++i) {
    let threw = false;
    try {
        maybeCallThrowing(throwers[i & 1], 0);
    } catch (error) {
        shouldBe(error.message, (i & 1 ? "b" : "a") + 0);
        threw = true;
    }
    shouldBe(threw, true);
}
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(maybeCallThrowing(throwers[i & 1], 10), 45);

function expectedAfterReplacement(object, x) {
    if (object instanceof B)
        return object.value * x + 1000;
    return expected(object, x);
}

function tailCall(object, x) {
    "use strict";
    return object.run(x);
}
noInline(tailCall);
function callTailCall(object, x) {
    return tailCall(object, x) + 1;
}
noInline(callTailCall);
for (let i = 0; i < testLoopCount; ++i) {
    let object = objects[i % objects.length];
    shouldBe(tailCall(object, i), expectedAfterReplacement(object, i));
    shouldBe(callTailCall(object, i), expectedAfterReplacement(object, i) + 1);
}
shouldBe(tailCall(f, 3), 703);
shouldBe(callTailCall(f, 3), 704);

// A case whose callee is the running function turns its tail call into a jump.
class CountDownA {
    step(objects, n, sum) {
        if (!n)
            return sum;
        let next = objects[n & 1];
        let m = n - 1;
        let t = sum + 1;
        return next.step(objects, m, t);
    }
}
class CountDownB {
    constructor() { this.extra = 0; }
    step(objects, n, sum) {
        if (!n)
            return sum;
        let next = objects[n & 1];
        let m = n - 1;
        let t = sum + 2;
        return next.step(objects, m, t);
    }
}
let countDownObjects = [new CountDownA, new CountDownB];
function countDownExpected(n) {
    let sum = 0;
    let current = 0;
    for (; n; --n) {
        sum += current ? 2 : 1;
        current = n & 1;
    }
    return sum;
}
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(countDownObjects[0].step(countDownObjects, i % 20, 0), countDownExpected(i % 20));

function sumInLoop(array) {
    let s = 0;
    for (let i = 0; i < 4; ++i)
        s += array[i].run(i);
    return s;
}
noInline(sumInLoop);
let loopObjects = [new A(1), new B(2), new A(3), new B(4)];
let loopExpected = 0;
for (let i = 0; i < 4; ++i)
    loopExpected += expectedAfterReplacement(loopObjects[i], i);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(sumInLoop(loopObjects), loopExpected);
shouldBe(sumInLoop([new A(1), new B(2), f, new B(4)]), expectedAfterReplacement(loopObjects[0], 0) + expectedAfterReplacement(loopObjects[1], 1) + 702 + expectedAfterReplacement(loopObjects[3], 3));

// One long-running call enters optimized code in the middle of the loop.
function accumulate(array, n) {
    let s = 0;
    for (let i = 0; i < n; ++i) {
        let r = array[i & 1].run(i);
        s = (s + r) | 0;
    }
    return s;
}
noInline(accumulate);
let accumulateObjects = [new A(1), new C(3)];
let accumulateExpected = 0;
for (let i = 0; i < testLoopCount * 50; ++i)
    accumulateExpected = (accumulateExpected + expected(accumulateObjects[i & 1], i)) | 0;
shouldBe(accumulate(accumulateObjects, testLoopCount * 50), accumulateExpected);
shouldBe(accumulate([new A(1), f], 2), expected(accumulateObjects[0], 0) + 701);

// Methods of classes made by one factory share a FunctionExecutable, so they share one case.
function makeClass(k) {
    return class {
        constructor(value) { this.value = value; }
        run(x) { return this.value * k + x; }
    };
}
let G0 = makeClass(10);
let G1 = makeClass(20);
let G2 = makeClass(30);
function factoryExpected(object, x) {
    if (object instanceof G0)
        return object.value * 10 + x;
    if (object instanceof G1)
        return object.value * 20 + x;
    if (object instanceof G2)
        return object.value * 30 + x;
    return expectedAfterReplacement(object, x);
}
function callFactory(object, x) {
    return object.run(x);
}
noInline(callFactory);
let factoryObjects = [new G0(1), new G1(2), new A(3), new G2(4), new C(5)];
for (let i = 0; i < testLoopCount; ++i) {
    let object = factoryObjects[i % factoryObjects.length];
    shouldBe(callFactory(object, i), factoryExpected(object, i));
}
shouldBe(callFactory(f, 3), 703);

// Each inlined copy of runOn dispatches on a fresh allocation, which can be sunk.
class H {
    constructor(value) { this.value = value; }
    run(x) { return this; }
}
function runOn(object, x) {
    return object.run(x);
}
function sinkA(x) { return runOn(new A(x), x); }
function sinkC(x) { return runOn(new C(x), 1); }
function sinkH(x) { return runOn(new H(x), x).value; }
noInline(sinkA);
noInline(sinkC);
noInline(sinkH);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sinkA(i), i + i);
    shouldBe(sinkC(i), i - 1);
    shouldBe(sinkH(i), i);
}

// runOnLarge is too large for DFG to inline but not for FTL, so FTL sees the polymorphic profile of its
// call site even where the receiver is a fresh allocation.
class P {
    constructor(value) { this.value = value; }
    run(x) { return this.value + x; }
}
class Q {
    constructor(value) { this.other = 0; this.value = value; }
    run(x) { return this.value - x; }
}
{
    // Adding a property to an instance makes the structures above unwatchable.
    let p = new P(1);
    p.extra = 1;
    let q = new Q(1);
    q.extra = 1;
}
function runOnLarge(object, x, y) {
    let t = x;
    if (y > 100) t = t * 3 + y;
    if (y > 200) t = t * 5 + y;
    if (y > 300) t = t * 7 + y;
    if (y > 400) t = t * 11 + y;
    if (y > 500) t = t * 13 + y;
    if (y > 600) t = t * 17 + y;
    if (y > 700) t = t * 19 + y;
    if (y > 800) t = t * 23 + y;
    return object.run(t);
}
function opaque() { }
noInline(opaque);
function sinkKnown(x) {
    let object = new P(x);
    return runOnLarge(object, x, 0);
}
function sinkAfterCall(x) {
    let object = new P(x);
    opaque();
    return runOnLarge(object, x, 0);
}
noInline(sinkKnown);
noInline(sinkAfterCall);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sinkKnown(i), i + i);
    shouldBe(sinkAfterCall(i), i + i);
    shouldBe(runOnLarge(new Q(i), 1, 0), i - 1);
}
