function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, errorType) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error('bad error: ' + error);
}

function set(target, key, value, receiver) {
    return Reflect.set(target, key, value, receiver);
}
noInline(set);

var setterReceiver = null;
class Base {
    set accessor(value) { setterReceiver = this; }
}
class Derived extends Base { }

for (var i = 0; i < testLoopCount; ++i) {
    var target = new Derived();
    target.data = 0;
    Object.defineProperty(target, 'readOnly', { value: 1, writable: false });
    var proxy = new Proxy(target, { });

    shouldBe(set(target, 'data', i, proxy), true);
    shouldBe(target.data, i);
    shouldBe(set(target, 'added', i, proxy), true);
    shouldBe(target.added, i);
    shouldBe(set(target, 'readOnly', 2, proxy), false);
    shouldBe(target.readOnly, 1);
    setterReceiver = null;
    shouldBe(set(target, 'accessor', i, proxy), true);
    shouldBe(setterReceiver, proxy);

    var array = [1, 2, 3];
    var arrayProxy = new Proxy(array, { });
    shouldBe(set(array, '1', 20, arrayProxy), true);
    shouldBe(set(array, 'length', 2, arrayProxy), true);
    shouldBe(array.length, 2);
    shouldBe(array[1], 20);
    shouldBe(set(array, '5', 6, arrayProxy), true);
    shouldBe(array.length, 6);

    var other = new Derived();
    shouldBe(set(target, 'data', 'other', new Proxy(other, { })), true);
    shouldBe(other.data, 'other');
    shouldBe(target.data, i);
}

var defineCount = 0;
var descriptorCount = 0;
var handler = { };
var target = { data: 0 };
var proxy = new Proxy(target, handler);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(set(target, 'data', i, proxy), true);

handler.defineProperty = function (t, key, descriptor) {
    ++defineCount;
    return Reflect.defineProperty(t, key, descriptor);
};
for (var i = 0; i < 10; ++i)
    shouldBe(set(target, 'data', i, proxy), true);
shouldBe(defineCount, 10);
shouldBe(target.data, 9);

handler.getOwnPropertyDescriptor = function (t, key) {
    ++descriptorCount;
    return Reflect.getOwnPropertyDescriptor(t, key);
};
for (var i = 0; i < 10; ++i)
    shouldBe(set(target, 'data', i, proxy), true);
shouldBe(descriptorCount, 10);
shouldBe(defineCount, 20);

var revocable = Proxy.revocable(target, { });
shouldBe(set(target, 'data', 1, revocable.proxy), true);
revocable.revoke();
shouldThrow(() => set(target, 'data', 2, revocable.proxy), TypeError);

for (var i = 0; i < testLoopCount; ++i) {
    var stringChild = Object.create(new String('abc'));
    var stringProxy = new Proxy(stringChild, { });
    shouldBe(set(stringChild, 'w', 1, stringProxy), true);
    shouldBe(set(stringChild, '1', 'x', stringProxy), false);
    shouldBe(Object.hasOwn(stringChild, '1'), false);

    var typedArray = new Uint8Array(4);
    var typedArrayChild = Object.create(typedArray);
    var typedArrayProxy = new Proxy(typedArrayChild, { });
    shouldBe(set(typedArrayChild, 'w', 1, typedArrayProxy), true);
    shouldBe(set(typedArrayChild, '10', 1, typedArrayProxy), true);
    shouldBe(Object.hasOwn(typedArrayChild, '10'), false);
    shouldBe(set(typedArrayChild, '1', 7, typedArrayProxy), true);
    shouldBe(Object.getOwnPropertyDescriptor(typedArrayChild, '1').value, 7);
    shouldBe(typedArray[1], 0);

    var frozen = Object.freeze({ data: 1 });
    var frozenProxy = new Proxy(frozen, { });
    shouldBe(set({ data: 0 }, 'data', 2, frozenProxy), false);
    shouldBe(set({ }, 'added', 2, frozenProxy), false);
    shouldBe(frozen.data, 1);

    var accessorTarget = { get data() { return 0; }, set data(value) { throw new Error('setter on the receiver must not be called'); } };
    shouldBe(set({ data: 0 }, 'data', 2, new Proxy(accessorTarget, { })), false);

    var nested = { };
    var nestedProxy = new Proxy(new Proxy(nested, { }), { });
    shouldBe(set({ }, 'data', i, nestedProxy), true);
    shouldBe(nested.data, i);
}

class SuperStore {
    store(value) { super.stored = value; }
}
for (var i = 0; i < testLoopCount; ++i) {
    var instance = new SuperStore();
    var instanceProxy = new Proxy(instance, { });
    instanceProxy.store(i);
    shouldBe(instance.stored, i);
}

var protoSetterReceiver = null;
Object.defineProperty(Object.prototype, 'protoSetter', { set(value) { protoSetterReceiver = this; }, configurable: true });
var plain = { };
var plainProxy = new Proxy(plain, { });
for (var i = 0; i < testLoopCount; ++i) {
    protoSetterReceiver = null;
    shouldBe(set(plain, 'protoSetter', i, plainProxy), true);
    shouldBe(protoSetterReceiver, plainProxy);
    shouldBe(Object.hasOwn(plain, 'protoSetter'), false);
}
delete Object.prototype.protoSetter;

var inheritedTrapCount = 0;
var warmTarget = { data: 0 };
var warmProxy = new Proxy(warmTarget, { });
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(set(warmTarget, 'data', i, warmProxy), true);
    shouldBe(set(warmTarget, i & 1 ? '0' : 'data', i, warmProxy), true);
}
Object.prototype.defineProperty = function (t, key, descriptor) {
    ++inheritedTrapCount;
    return Reflect.defineProperty(t, key, descriptor);
};
shouldBe(set(warmTarget, 'data', 1, warmProxy), true);
shouldBe(set(warmTarget, '0', 1, warmProxy), true);
shouldBe(inheritedTrapCount, 2);
delete Object.prototype.defineProperty;

function setThroughReflect(receiver, key, value) {
    return Reflect.set({ }, key, value, receiver);
}
noInline(setThroughReflect);

function setThroughSloppyAssignment(receiver, key, value) {
    receiver[key] = value;
    return true;
}
noInline(setThroughSloppyAssignment);

function setThroughStrictAssignment(receiver, key, value) {
    "use strict";
    receiver[key] = value;
    return true;
}
noInline(setThroughStrictAssignment);

function testInnerTrapChangesOuterProxy(store, key, mutation) {
    var inner = { };
    var outerHandler = { };
    var mutate = false;
    var revoke;
    var outerDefineCount = 0;
    var innerProxy = new Proxy(inner, {
        getOwnPropertyDescriptor(t, k) {
            if (mutate) {
                if (mutation === 'trap') {
                    outerHandler.defineProperty = function (t, k, descriptor) {
                        ++outerDefineCount;
                        return Reflect.defineProperty(t, k, descriptor);
                    };
                } else
                    revoke();
            }
            return Reflect.getOwnPropertyDescriptor(t, k);
        }
    });
    var revocable = Proxy.revocable(innerProxy, outerHandler);
    revoke = revocable.revoke;
    var outerProxy = revocable.proxy;

    var iterations = Math.ceil(testLoopCount / 3);
    for (var i = 0; i < iterations; ++i) {
        shouldBe(store(outerProxy, key, i), true);
        shouldBe(inner[key], i);
    }
    shouldBe(outerDefineCount, 0);

    mutate = true;
    if (mutation === 'trap') {
        shouldBe(store(outerProxy, key, 42), true);
        shouldBe(outerDefineCount, 1);
        shouldBe(inner[key], 42);
    } else {
        shouldThrow(() => store(outerProxy, key, 42), TypeError);
        shouldBe(inner[key], iterations - 1);
    }
}
for (var store of [setThroughReflect, setThroughSloppyAssignment, setThroughStrictAssignment]) {
    for (var key of ['named', '0']) {
        for (var mutation of ['trap', 'revoke'])
            testInnerTrapChangesOuterProxy(store, key, mutation);
    }
}
