function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function thrownBy(func)
{
    try {
        func();
    } catch (error) {
        return error;
    }
    throw new Error("did not throw");
}

function Other() { }

// A built-in target gets its [[Prototype]] from newTarget but its internal slots from the target.

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

(function array() {
    class MyArray extends Array { }

    function withLength(length) { return Reflect.construct(Array, [length]); }
    function withLengthAndSameNewTarget(length) { return Reflect.construct(Array, [length], Array); }
    function withLengthAndNewTarget(length, newTarget) { return Reflect.construct(Array, [length], newTarget); }
    function withElements(a, b) { return Reflect.construct(Array, [a, b]); }
    function withElementsAndNewTarget(a, b, newTarget) { return Reflect.construct(Array, [a, b], newTarget); }
    function empty() { return Reflect.construct(Array, []); }
    function subclass(a, b) { return Reflect.construct(MyArray, [a, b]); }
    for (let func of [withLength, withLengthAndSameNewTarget, withLengthAndNewTarget, withElements, withElementsAndNewTarget, empty, subclass])
        noInline(func);

    for (let i = 0; i < testLoopCount; ++i) {
        let array = withLength(i & 15);
        shouldBe(array.length, i & 15);
        shouldBe(Object.getPrototypeOf(array), Array.prototype);
        shouldBe(Array.isArray(array), true);

        array = withLengthAndSameNewTarget(3);
        shouldBe(array.length, 3);
        shouldBe(Object.getPrototypeOf(array), Array.prototype);

        array = withLengthAndNewTarget(4, i & 1 ? MyArray : Other);
        shouldBe(array.length, 4);
        shouldBe(Array.isArray(array), true);
        shouldBe(Object.getPrototypeOf(array), i & 1 ? MyArray.prototype : Other.prototype);

        array = withElements(i, "b");
        shouldBe(array.length, 2);
        shouldBe(array[0], i);
        shouldBe(array[1], "b");

        array = withElementsAndNewTarget(i, "b", Other);
        shouldBe(array.length, 2);
        shouldBe(array[0], i);
        shouldBe(Object.getPrototypeOf(array), Other.prototype);

        shouldBe(empty().length, 0);

        array = subclass(i, 2);
        shouldBe(array.length, 2);
        shouldBe(array instanceof MyArray, true);

        if (i % rareStride)
            continue;
        shouldBe(withLength("3").length, 1);
        shouldBe(withLength("x")[0], "x");
        shouldBe(thrownBy(() => withLength(-1)) instanceof RangeError, true);
        shouldBe(thrownBy(() => withLength(1.5)) instanceof RangeError, true);
        shouldBe(thrownBy(() => withLengthAndNewTarget(-1, Other)) instanceof RangeError, true);
    }
})();

(function object() {
    function withoutNewTarget(value) { return Reflect.construct(Object, [value]); }
    function sameNewTarget(value) { return Reflect.construct(Object, [value], Object); }
    function otherNewTarget(value, newTarget) { return Reflect.construct(Object, [value], newTarget); }
    function empty() { return Reflect.construct(Object, []); }
    for (let func of [withoutNewTarget, sameNewTarget, otherNewTarget, empty])
        noInline(func);

    let existing = { existing: true };

    for (let i = 0; i < testLoopCount; ++i) {
        // With newTarget being Object itself the argument is converted with ToObject.
        shouldBe(withoutNewTarget(existing), existing);
        shouldBe(sameNewTarget(existing), existing);
        shouldBe(Object.getPrototypeOf(empty()), Object.prototype);

        // With any other newTarget the argument is ignored and an ordinary object is created from newTarget.
        let object = otherNewTarget(existing, Other);
        shouldBe(object === existing, false);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);

        if (i % rareStride)
            continue;
        shouldBe(typeof withoutNewTarget(i), "object");
        shouldBe(withoutNewTarget(i) instanceof Number, true);
        shouldBe(withoutNewTarget(i).valueOf(), i);
        shouldBe(withoutNewTarget("s") instanceof String, true);
        shouldBe(Object.getPrototypeOf(withoutNewTarget(null)), Object.prototype);
        shouldBe(Object.getPrototypeOf(withoutNewTarget(undefined)), Object.prototype);
        shouldBe(otherNewTarget(i, Other) instanceof Number, false);
        shouldBe(Object.getPrototypeOf(otherNewTarget(i, Other)), Other.prototype);
    }
})();

(function wrappers() {
    function string(value) { return Reflect.construct(String, [value]); }
    function stringWithNewTarget(value, newTarget) { return Reflect.construct(String, [value], newTarget); }
    function emptyString() { return Reflect.construct(String, []); }
    function number(value) { return Reflect.construct(Number, [value]); }
    function numberWithNewTarget(value, newTarget) { return Reflect.construct(Number, [value], newTarget); }
    function boolean(value) { return Reflect.construct(Boolean, [value]); }
    function symbol(value) { return Reflect.construct(Symbol, [value]); }
    function bigInt(value) { return Reflect.construct(BigInt, [value]); }
    for (let func of [string, stringWithNewTarget, emptyString, number, numberWithNewTarget, boolean, symbol, bigInt])
        noInline(func);

    for (let i = 0; i < testLoopCount; ++i) {
        // Unlike a call, constructing makes wrapper objects.
        let object = string(i);
        shouldBe(typeof object, "object");
        shouldBe(object.valueOf(), String(i));
        shouldBe(object.length, String(i).length);
        shouldBe(Object.getPrototypeOf(object), String.prototype);

        object = stringWithNewTarget("abc", Other);
        shouldBe(typeof object, "object");
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(String.prototype.valueOf.call(object), "abc");
        shouldBe(object.length, 3);
        shouldBe(object[1], "b");

        shouldBe(emptyString().valueOf(), "");

        object = number(i);
        shouldBe(typeof object, "object");
        shouldBe(object.valueOf(), i);

        object = numberWithNewTarget("12", Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Number.prototype.valueOf.call(object), 12);

        object = boolean(i);
        shouldBe(typeof object, "object");
        shouldBe(object.valueOf(), !!i);

        if (i % rareStride)
            continue;
        // Symbol and BigInt are constructors as far as IsConstructor goes, and throw from their own [[Construct]].
        shouldBe(thrownBy(() => symbol("s")) instanceof TypeError, true);
        shouldBe(thrownBy(() => bigInt(1)) instanceof TypeError, true);
        shouldBe(thrownBy(() => string(Symbol())) instanceof TypeError, true);
        shouldBe(string({ toString() { return "converted"; } }).valueOf(), "converted");
    }
})();

(function collections() {
    class MyMap extends Map { }

    function map() { return Reflect.construct(Map, []); }
    function mapWithNewTarget(newTarget) { return Reflect.construct(Map, [], newTarget); }
    function mapWithEntries(entries) { return Reflect.construct(Map, [entries]); }
    function set() { return Reflect.construct(Set, []); }
    function setWithNewTarget(newTarget) { return Reflect.construct(Set, [], newTarget); }
    function weakMap() { return Reflect.construct(WeakMap, []); }
    function weakMapWithNewTarget(newTarget) { return Reflect.construct(WeakMap, [], newTarget); }
    function weakSet() { return Reflect.construct(WeakSet, []); }
    function weakSetWithNewTarget(newTarget) { return Reflect.construct(WeakSet, [], newTarget); }
    for (let func of [map, mapWithNewTarget, mapWithEntries, set, setWithNewTarget, weakMap, weakMapWithNewTarget, weakSet, weakSetWithNewTarget])
        noInline(func);

    let key = { };

    for (let i = 0; i < testLoopCount; ++i) {
        let object = map();
        shouldBe(Object.getPrototypeOf(object), Map.prototype);
        shouldBe(object.set(i, i).get(i), i);

        object = mapWithNewTarget(i & 1 ? MyMap : Set);
        shouldBe(Object.getPrototypeOf(object), i & 1 ? MyMap.prototype : Set.prototype);
        shouldBe(Map.prototype.set.call(object, i, "v"), object);
        shouldBe(Map.prototype.get.call(object, i), "v");
        if (!(i & 1))
            shouldBe(thrownBy(() => object.add(1)) instanceof TypeError, true);

        shouldBe(mapWithEntries([[i, "entry"]]).get(i), "entry");

        object = set();
        shouldBe(Object.getPrototypeOf(object), Set.prototype);
        shouldBe(object.add(i).has(i), true);

        object = setWithNewTarget(Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Set.prototype.has.call(Set.prototype.add.call(object, i), i), true);

        shouldBe(weakMap().set(key, i).get(key), i);
        object = weakMapWithNewTarget(Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(WeakMap.prototype.has.call(object, key), false);

        shouldBe(weakSet().add(key).has(key), true);
        object = weakSetWithNewTarget(Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(WeakSet.prototype.has.call(object, key), false);
    }
})();

(function buffersAndTypedArrays() {
    function arrayBuffer(length) { return Reflect.construct(ArrayBuffer, [length]); }
    function arrayBufferWithNewTarget(length, newTarget) { return Reflect.construct(ArrayBuffer, [length], newTarget); }
    function typedArray(length) { return Reflect.construct(Int32Array, [length]); }
    function typedArrayWithNewTarget(length, newTarget) { return Reflect.construct(Int32Array, [length], newTarget); }
    function typedArrayFromArray(a, b) { return Reflect.construct(Float64Array, [[a, b]]); }
    function dataView(buffer) { return Reflect.construct(DataView, [buffer], Other); }
    for (let func of [arrayBuffer, arrayBufferWithNewTarget, typedArray, typedArrayWithNewTarget, typedArrayFromArray, dataView])
        noInline(func);

    let buffer = new ArrayBuffer(8);

    for (let i = 0; i < testLoopCount; ++i) {
        let object = arrayBuffer(i & 31);
        shouldBe(object.byteLength, i & 31);
        shouldBe(Object.getPrototypeOf(object), ArrayBuffer.prototype);

        object = arrayBufferWithNewTarget(8, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "byteLength").get.call(object), 8);

        object = typedArray(i & 15);
        shouldBe(object.length, i & 15);
        shouldBe(Object.getPrototypeOf(object), Int32Array.prototype);

        object = typedArrayWithNewTarget(4, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Object.getOwnPropertyDescriptor(Object.getPrototypeOf(Int32Array.prototype), "length").get.call(object), 4);
        object[0] = i;
        shouldBe(object[0], i);

        object = typedArrayFromArray(i, 0.5);
        shouldBe(object[0], i);
        shouldBe(object[1], 0.5);

        if (i % rareStride)
            continue;
        object = dataView(buffer);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(DataView.prototype.getInt8.call(object, 0), 0);
        shouldBe(thrownBy(() => typedArray(-1)) instanceof RangeError, true);
        shouldBe(thrownBy(() => arrayBuffer(-1)) instanceof RangeError, true);
    }
})();

(function otherBuiltins() {
    class MyError extends Error { }
    class MyPromise extends Promise { }

    function regExp(pattern, flags) { return Reflect.construct(RegExp, [pattern, flags]); }
    function regExpWithNewTarget(pattern, newTarget) { return Reflect.construct(RegExp, [pattern], newTarget); }
    function regExpFromRegExp(regExp) { return Reflect.construct(RegExp, [regExp]); }
    function date(time) { return Reflect.construct(Date, [time]); }
    function dateWithNewTarget(time, newTarget) { return Reflect.construct(Date, [time], newTarget); }
    function error(message) { return Reflect.construct(Error, [message], MyError); }
    function typeError(message) { return Reflect.construct(TypeError, [message], Other); }
    function promise(executor) { return Reflect.construct(Promise, [executor], MyPromise); }
    function functionConstructor(body) { return Reflect.construct(Function, ["a", body], Other); }
    function proxy(target, handler) { return Reflect.construct(Proxy, [target, handler]); }
    for (let func of [regExp, regExpWithNewTarget, regExpFromRegExp, date, dateWithNewTarget, error, typeError, promise, functionConstructor, proxy])
        noInline(func);

    let existingRegExp = /existing/g;

    for (let i = 0; i < testLoopCount; ++i) {
        let object = regExp("a+", "g");
        shouldBe(Object.getPrototypeOf(object), RegExp.prototype);
        shouldBe(object.test("caat"), true);
        shouldBe(object.lastIndex, 3);

        object = regExpWithNewTarget("b", Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(RegExp.prototype.test.call(object, "abc"), true);

        // IsRegExp(pattern) is true and newTarget is RegExp, but pattern.constructor only matters for a call.
        object = regExpFromRegExp(existingRegExp);
        shouldBe(object === existingRegExp, false);
        shouldBe(object.source, "existing");

        object = date(i);
        shouldBe(object.getTime(), i);
        object = dateWithNewTarget(i, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Date.prototype.getTime.call(object), i);

        object = error("message");
        shouldBe(Object.getPrototypeOf(object), MyError.prototype);
        shouldBe(object.message, "message");
        shouldBe(Object.getPrototypeOf(typeError("m")), Other.prototype);

        if (i % rareStride)
            continue;
        let resolved;
        object = promise((resolve) => { resolved = resolve; });
        shouldBe(Object.getPrototypeOf(object), MyPromise.prototype);
        shouldBe(typeof resolved, "function");
        shouldBe(thrownBy(() => promise(1)) instanceof TypeError, true);

        object = functionConstructor("return a + 1");
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(Function.prototype.call.call(object, null, 1), 2);

        object = proxy({ target: true }, { });
        shouldBe(object.target, true);
        shouldBe(thrownBy(() => proxy(1, { })) instanceof TypeError, true);
    }
})();
