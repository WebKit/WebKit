function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`expected ${errorType.name} but got ${String(error)}`);
}

function boundFunctionWithRevokedTarget() {
    let { proxy, revoke } = Proxy.revocable(function () { }, {});
    let bound = proxy.bind();
    revoke();
    return bound;
}

function proxyRevokedByPrototypeGetter(prototype) {
    let count = 0;
    let { proxy, revoke } = Proxy.revocable(function () { }, {
        get(target, key, receiver) {
            if (key === "prototype") {
                count++;
                revoke();
                return prototype;
            }
            return Reflect.get(target, key, receiver);
        }
    });
    return { proxy, count: () => count };
}

const other = createGlobalObject();
const GeneratorFunction = function* () { }.constructor;
const AsyncFunction = async function () { }.constructor;
const AsyncGeneratorFunction = async function* () { }.constructor;

const tests = [
    [Object, [], other.Object.prototype],
    [Function, [], other.Function.prototype],
    [GeneratorFunction, [], other.eval("(function* () { })").constructor.prototype],
    [AsyncFunction, [], other.eval("(async function () { })").constructor.prototype],
    [AsyncGeneratorFunction, [], other.eval("(async function* () { })").constructor.prototype],
    [Array, [], other.Array.prototype],
    [Array, [3], other.Array.prototype],
    [Array, [1, 2], other.Array.prototype],
    [Boolean, [], other.Boolean.prototype],
    [Number, [], other.Number.prototype],
    [String, [], other.String.prototype],
    [Date, [], other.Date.prototype],
    [RegExp, [], other.RegExp.prototype],
    [Error, [], other.Error.prototype],
    [TypeError, [], other.TypeError.prototype],
    [AggregateError, [[]], other.AggregateError.prototype],
    [Map, [], other.Map.prototype],
    [Set, [], other.Set.prototype],
    [WeakMap, [], other.WeakMap.prototype],
    [WeakSet, [], other.WeakSet.prototype],
    [WeakRef, [{}], other.WeakRef.prototype],
    [FinalizationRegistry, [function () { }], other.FinalizationRegistry.prototype],
    [ArrayBuffer, [8], other.ArrayBuffer.prototype],
    [DataView, [new ArrayBuffer(8)], other.DataView.prototype],
    [Int8Array, [1], other.Int8Array.prototype],
    [Float64Array, [[1, 2]], other.Float64Array.prototype],
    [Promise, [function () { }], other.Promise.prototype],
    [Intl.Collator, [], other.Intl.Collator.prototype],
    [Intl.NumberFormat, [], other.Intl.NumberFormat.prototype],
    [function () { }, [], other.Object.prototype],
    [class { }, [], other.Object.prototype],
    [class extends Array { }, [], other.Array.prototype],
    [class extends Promise { }, [function () { }], other.Promise.prototype],
];

for (let [constructor, args, otherPrototype] of tests) {
    let prototype = {};

    let bound = boundFunctionWithRevokedTarget();
    bound.prototype = prototype;
    shouldBe(Object.getPrototypeOf(Reflect.construct(constructor, args, bound)), prototype);

    shouldThrow(() => Reflect.construct(constructor, args, boundFunctionWithRevokedTarget()), TypeError);

    let revokedReturningObject = proxyRevokedByPrototypeGetter(prototype);
    shouldBe(Object.getPrototypeOf(Reflect.construct(constructor, args, revokedReturningObject.proxy)), prototype);
    shouldBe(revokedReturningObject.count(), 1);

    let revokedReturningUndefined = proxyRevokedByPrototypeGetter(undefined);
    shouldThrow(() => Reflect.construct(constructor, args, revokedReturningUndefined.proxy), TypeError);
    shouldBe(revokedReturningUndefined.count(), 1);

    let otherBound = new other.Function().bind();
    shouldBe(Object.getPrototypeOf(Reflect.construct(constructor, args, otherBound)), otherPrototype);

    let otherProxy = new Proxy(otherBound, {});
    shouldBe(Object.getPrototypeOf(Reflect.construct(constructor, args, otherProxy)), otherPrototype);
}
