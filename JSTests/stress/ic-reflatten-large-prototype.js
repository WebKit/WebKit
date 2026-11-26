function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function Base() { }
for (let i = 0; i < 200; ++i)
    Object.defineProperty(Base.prototype, "p" + i, { value: i, writable: true, configurable: true });

function Derived() { }
Derived.prototype = Object.create(Base.prototype);
Derived.prototype.own = "own";

function getP5(o) { return o.p5; }
function getLate(o) { return o.late; }
function getX(o, k) { return o["x" + k]; }
function getMissing(o) { return o.missing; }
noInline(getP5);
noInline(getLate);
noInline(getX);
noInline(getMissing);

const object = new Derived;
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(getP5(object), 5);
    shouldBe(getMissing(object), undefined);
}

Object.defineProperty(Base.prototype, "late", { get() { return 42; }, configurable: true });
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(getP5(object), 5);
    shouldBe(getLate(object), 42);
    shouldBe(getMissing(object), undefined);
}

Derived.prototype.p5 = 100;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(getP5(object), 100);
delete Derived.prototype.p5;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(getP5(object), 5);

Base.prototype.missing = "found";
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(getMissing(object), "found");

for (let k = 0; k < 10; ++k) {
    Object.defineProperty(Base.prototype, "x" + k, { value: k, configurable: true });
    Object.defineProperty(Base.prototype, "late", { get() { return k; }, configurable: true });
    for (let i = 0; i < 1000; ++i) {
        shouldBe(getX(object, k), k);
        shouldBe(getLate(object), k);
        shouldBe(getP5(object), 5);
    }
}

delete Base.prototype.p5;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(getP5(object), undefined);
