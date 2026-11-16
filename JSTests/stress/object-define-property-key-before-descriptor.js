function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function defineValue(mutate)
{
    var descriptor = { value: 0x1337 };
    var key = {
        toString() {
            if (mutate)
                descriptor.value = 0x4141;
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineValue);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineValue(false), 0x1337);
shouldBe(defineValue(true), 0x4141);

function defineSeed(seed, mutate)
{
    var descriptor = { value: seed };
    var key = {
        toString() {
            if (mutate)
                descriptor.value = seed + 1;
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineSeed);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineSeed(i, false), i);
shouldBe(defineSeed(50, true), 51);

function defineWritable(mutate)
{
    var descriptor = { value: 1, writable: true };
    var key = {
        toString() {
            if (mutate)
                descriptor.writable = false;
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return Object.getOwnPropertyDescriptor(object, "slot").writable;
}
noInline(defineWritable);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineWritable(false), true);
shouldBe(defineWritable(true), false);

function defineGetter(mutate)
{
    function first() { return 1; }
    function second() { return 2; }
    var descriptor = { get: first };
    var key = {
        toString() {
            if (mutate)
                descriptor.get = second;
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineGetter);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineGetter(false), 1);
shouldBe(defineGetter(true), 2);

function defineNullPrototype(mutate)
{
    var descriptor = Object.create(null);
    descriptor.value = 0x1337;
    var key = {
        toString() {
            if (mutate)
                descriptor.value = 0x4141;
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineNullPrototype);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineNullPrototype(false), 0x1337);
shouldBe(defineNullPrototype(true), 0x4141);

function defineTransition(mutate)
{
    var descriptor = { value: 1 };
    var key = {
        toString() {
            if (mutate) {
                delete descriptor.value;
                descriptor.value = 2;
            }
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineTransition);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineTransition(false), 1);
shouldBe(defineTransition(true), 2);

function defineString(key, value)
{
    var object = {};
    Object.defineProperty(object, key, { value: value });
    return object[key];
}
noInline(defineString);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineString("slot", i), i);

function defineIndex(index)
{
    var object = {};
    Object.defineProperty(object, index, { value: index + 1 });
    return object[index];
}
noInline(defineIndex);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineIndex(i), i + 1);

function defineFlex(useObject)
{
    var descriptor = { value: 1 };
    var key = "slot";
    if (useObject) {
        key = {
            toString() {
                descriptor.value = 2;
                return "slot";
            }
        };
    }
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineFlex);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineFlex(false), 1);
shouldBe(defineFlex(true), 2);

function defineThrows(shouldThrow)
{
    var descriptor = { value: 1 };
    var key = {
        toString() {
            if (shouldThrow)
                throw new Error("boom");
            return "slot";
        }
    };
    var object = {};
    Object.defineProperty(object, key, descriptor);
    return object.slot;
}
noInline(defineThrows);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(defineThrows(false), 1);
var threw = false;
try {
    defineThrows(true);
} catch (e) {
    threw = e.message === "boom";
}
if (!threw)
    throw new Error("expected toString to throw");
