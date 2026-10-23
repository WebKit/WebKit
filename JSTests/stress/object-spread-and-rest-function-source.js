function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function describe(object) {
    return Reflect.ownKeys(object).map((key) => {
        let descriptor = Object.getOwnPropertyDescriptor(object, key);
        shouldBe(descriptor.writable && descriptor.enumerable && descriptor.configurable, true);
        return String(key) + "=" + String(descriptor.value);
    }).join();
}
noInline(describe);

function spreadOnly(source) {
    return { ...source };
}
noInline(spreadOnly);

function spreadFirst(source) {
    return { ...source, x: "x" };
}
noInline(spreadFirst);

function spreadLast(source) {
    return { x: "x", ...source };
}
noInline(spreadLast);

function restOnly(source) {
    let { ...rest } = source;
    return rest;
}
noInline(restOnly);

function restExcludingA(source) {
    let { a, ...rest } = source;
    return rest;
}
noInline(restExcludingA);

function restExcludingComputed(source, key) {
    let { [key]: ignored, ...rest } = source;
    return rest;
}
noInline(restExcludingComputed);

function describeWithoutX(object) {
    shouldBe(object.x, "x");
    delete object.x;
    return describe(object);
}
noInline(describeWithoutX);

function check(source, expected) {
    shouldBe(describe(spreadOnly(source)), expected);
    shouldBe(describe(restOnly(source)), expected);
    shouldBe(describeWithoutX(spreadFirst(source)), expected);
    shouldBe(describeWithoutX(spreadLast(source)), expected);
}
noInline(check);

function withProperties(source) {
    source.a = 1;
    source.b = 2;
    source.c = 3;
    return source;
}

let factories = [
    () => function () { },
    () => function named(x, y) { },
    () => function () { "use strict"; },
    () => () => { },
    () => ({ method() { } }).method,
    () => Object.getOwnPropertyDescriptor({ get getter() { return 1; } }, "getter").get,
    () => class { },
    () => class Named { constructor(x) { } },
    () => class extends Object { },
    () => function* () { },
    () => async function () { },
    () => async function* () { },
    () => async () => { },
    () => function (x) { }.bind(null),
    () => { let result; new Promise((resolve) => { result = resolve; }); return result; },
    () => new Function("x", "return x"),
    () => new (class extends Function { }),
    () => createGlobalObject().Function("x", "return x"),
];

for (let factory of factories) {
    check(factory(), "");

    let source = withProperties(factory());
    check(source, "a=1,b=2,c=3");
    shouldBe(describe(restExcludingA(source)), "b=2,c=3");
    shouldBe(describe(restExcludingComputed(source, "b")), "a=1,c=3");
    shouldBe(describe(restExcludingComputed(source, "name")), "a=1,b=2,c=3");
    shouldBe(Object.getPrototypeOf(spreadOnly(source)), Object.prototype);
    shouldBe(Reflect.ownKeys(source).join().replace(",prototype", ""), "length,name,a,b,c");
    source.prototype;
    check(source, "a=1,b=2,c=3");
    Object.defineProperty(source, "name", { enumerable: true });
    shouldBe(Object.keys(spreadOnly(source)).join(), "name,a,b,c");
    shouldBe(Object.keys(restOnly(source)).join(), "name,a,b,c");
    Object.defineProperty(source, "length", { enumerable: true });
    shouldBe(Object.keys(spreadOnly(source)).join(), "length,name,a,b,c");
    shouldBe(Object.keys(restOnly(source)).join(), "length,name,a,b,c");
}

for (let i = 0; i < testLoopCount; ++i) {
    let source = withProperties(i & 1 ? function () { } : () => { });
    shouldBe(Object.keys(spreadOnly(source)).join(), "a,b,c");
    shouldBe(Object.keys(spreadFirst(source)).join(), "a,b,c,x");
    shouldBe(Object.keys(spreadLast(source)).join(), "x,a,b,c");
    shouldBe(Object.keys(restExcludingA(source)).join(), "b,c");
    shouldBe(Object.keys(spreadOnly(class { static a = 1; static name = "field"; })).join(), "name,a");
}

{
    let source = withProperties(function named() { });
    Object.defineProperty(source, "name", { enumerable: true });
    check(source, "name=named,a=1,b=2,c=3");
}

{
    let source = withProperties(function (x, y) { });
    Object.defineProperty(source, "length", { enumerable: true });
    check(source, "length=2,a=1,b=2,c=3");
}

{
    let source = withProperties((x) => { });
    Object.defineProperty(source, "name", { value: "defined", enumerable: true });
    Object.defineProperty(source, "length", { enumerable: true });
    check(source, "length=1,name=defined,a=1,b=2,c=3");
}

{
    let source = withProperties(function named() { });
    check(source, "a=1,b=2,c=3");
    Reflect.defineProperty(source, "name", { enumerable: true });
    check(source, "name=named,a=1,b=2,c=3");
}

{
    let source = withProperties(function named(x) { }.bind(null));
    Object.defineProperty(source, "name", { enumerable: true });
    check(source, "name=bound named,a=1,b=2,c=3");
}

check(class { static a = 1; static name = "field"; static b = 2; }, "name=field,a=1,b=2");
check(class { static a = 1; static length = "field"; static b = 2; }, "length=field,a=1,b=2");
check(class { static a = 1; static ["name"] = "field"; static ["length"] = "other"; }, "length=other,name=field,a=1");
check(class { static a = 1; static name() { } static length() { } static b = 2; }, "a=1,b=2");
check(class extends class { static inherited = 1; } { static a = 1; }, "a=1");

{
    let source = withProperties(function () { });
    Object.defineProperty(source, "hidden", { value: 1, enumerable: false });
    Object.defineProperty(source, "readOnly", { value: 2, enumerable: true });
    check(source, "a=1,b=2,c=3,readOnly=2");
}

{
    let source = function () { };
    source[Symbol("symbol")] = 1;
    source.a = 2;
    check(source, "a=2,Symbol(symbol)=1");
}

{
    let source = withProperties(function () { });
    source[1] = "one";
    source[0] = "zero";
    check(source, "0=zero,1=one,a=1,b=2,c=3");
}

{
    let source = withProperties(() => { });
    source.prototype = 4;
    check(source, "a=1,b=2,c=3,prototype=4");
}

{
    let source = withProperties(function () { });
    source.prototype = 4;
    check(source, "a=1,b=2,c=3");
}

{
    let source = withProperties(function () { });
    Object.defineProperty(source, "__proto__", { value: 4, writable: true, enumerable: true, configurable: true });
    check(source, "a=1,b=2,c=3,__proto__=4");
    shouldBe(describe({ get __proto__() { return "getter"; }, set __proto__(value) { throw new Error("setter"); }, ...source }), "__proto__=4,a=1,b=2,c=3");
}

check(Object.freeze(withProperties(function () { })), "a=1,b=2,c=3");
check(Object.seal(withProperties(function () { })), "a=1,b=2,c=3");

{
    let source = withProperties(function () { });
    delete source.b;
    delete source.name;
    delete source.length;
    check(source, "a=1,c=3");
}

{
    let source = function () { };
    for (let i = 0; i < 200; ++i)
        source["k" + i] = i;
    shouldBe(Object.keys(spreadOnly(source)).length, 200);
    shouldBe(Object.keys(restOnly(source)).length, 200);
    for (let i = 0; i < 200; ++i) {
        if (i % 50)
            delete source["k" + i];
    }
    check(source, "k0=0,k50=50,k100=100,k150=150");
}

{
    let log = [];
    let source = function () { };
    source.a = 1;
    Object.defineProperty(source, "b", { get() { log.push("b"); delete source.c; source.d = 4; return 2; }, enumerable: true, configurable: true });
    source.c = 3;
    shouldBe(describe(spreadOnly(source)), "a=1,b=2");
    shouldBe(log.join(), "b");
    shouldBe(describe(restOnly(source)), "a=1,b=2,d=4");
    shouldBe(log.join(), "b,b");
}

{
    class Private {
        static #field = 1;
        static #method() { }
        static a = 1;
        static hasField(object) { return #field in object; }
        static hasMethod(object) { return #method in object; }
    }
    shouldBe(Private.hasField(Private), true);
    shouldBe(Private.hasMethod(Private), true);
    for (let result of [spreadOnly(Private), spreadFirst(Private), spreadLast(Private), restOnly(Private)]) {
        shouldBe(result.a, 1);
        shouldBe(Private.hasField(result), false);
        shouldBe(Private.hasMethod(result), false);
    }
}

{
    let source = withProperties(function () { });
    let result = spreadOnly(source);
    result.a = 10;
    source.b = 20;
    shouldBe(describe(result), "a=10,b=2,c=3");
    shouldBe(describe(spreadOnly(source)), "a=1,b=20,c=3");
}

check(Number, "");
check(Promise, "");
check(Math.max, "");
check(Array.prototype.map, "");
