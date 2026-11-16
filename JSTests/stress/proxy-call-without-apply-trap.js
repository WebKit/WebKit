"use strict";

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`${message}: expected ${errorType.name} but got ${String(error)}`);
}

function target(a, b) {
    return [this, a, b, arguments.length];
}

function callProxy(proxy, thisValue, a, b) {
    const result = proxy.call(thisValue, a, b);
    return result;
}
noInline(callProxy);

function callDirect(proxy, a) {
    const result = proxy(a);
    return result;
}
noInline(callDirect);

function tailCall(proxy, a) {
    "use strict";
    return proxy(a);
}
noInline(tailCall);

function callMethod(object, a, b) {
    const result = object.method(a, b);
    return result;
}
noInline(callMethod);

function construct(proxy, a) {
    return new proxy(a);
}
noInline(construct);

{
    const proxy = new Proxy(target, { });
    const nested = new Proxy(new Proxy(target, { }), { get() { return 0; } });
    const thisValue = { };
    for (let i = 0; i < testLoopCount; ++i) {
        let result = callMethod({ method: proxy }, i, i + 1);
        shouldBe(result[1], i, "first argument");
        shouldBe(result[2], i + 1, "second argument");
        shouldBe(result[3], 2, "argument count");
        result = callDirect(proxy, i);
        shouldBe(result[0], undefined, "this of a plain call");
        shouldBe(result[2], undefined, "missing argument");
        shouldBe(result[3], 1, "argument count with missing argument");
        shouldBe(callProxy(proxy, thisValue, 1, 2)[0], thisValue, "this through Function.prototype.call");
        result = tailCall(proxy, i);
        shouldBe(result[0], undefined, "this of a tail call");
        shouldBe(result[3], 1, "argument count of a tail call");
        shouldBe(tailCall(nested, i)[1], i, "tail call through nested proxies");
    }
}

{
    let applyCount = 0;
    const handler = { };
    const proxy = new Proxy(target, handler);
    for (let i = 0; i < testLoopCount; ++i) {
        if (i === (testLoopCount >> 1))
            handler.apply = function (t, thisArgument, args) { ++applyCount; return "trapped"; };
        let result = callDirect(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "before adding the apply trap");
        else
            shouldBe(result, "trapped", "after adding the apply trap");
    }
    shouldBe(applyCount, testLoopCount - (testLoopCount >> 1), "apply trap count");
}

{
    const proxy = new Proxy(target, { });
    for (let i = 0; i < testLoopCount; ++i) {
        if (i === (testLoopCount >> 1))
            Object.prototype.apply = function () { return "from Object.prototype"; };
        let result = callDirect(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "before adding Object.prototype.apply");
        else
            shouldBe(result, "from Object.prototype", "after adding Object.prototype.apply");
    }
    delete Object.prototype.apply;
}

{
    let getterCount = 0;
    const handler = {
        get apply() {
            ++getterCount;
            return undefined;
        }
    };
    const proxy = new Proxy(target, handler);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(callDirect(proxy, i)[1], i, "apply getter returning undefined");
    shouldBe(getterCount, testLoopCount, "apply getter count");
}

{
    const { proxy, revoke } = Proxy.revocable(target, { });
    for (let i = 0; i < testLoopCount; ++i) {
        if (i === (testLoopCount >> 1))
            revoke();
        if (i < (testLoopCount >> 1))
            shouldBe(callDirect(proxy, i)[1], i, "before revocation");
        else
            shouldThrow(() => callDirect(proxy, i), TypeError, "after revocation");
    }
}

{
    class C { constructor(a) { this.a = a; } }
    const proxy = new Proxy(C, { });
    const nonCallable = new Proxy({ }, { });
    const native = new Proxy(Math.max, { });
    const bound = new Proxy(target.bind(null, "bound"), { });
    for (let i = 0; i < testLoopCount; ++i) {
        shouldThrow(() => callDirect(proxy, i), TypeError, "calling a class constructor");
        shouldBe(construct(proxy, i).a, i, "constructing through the proxy");
        shouldThrow(() => callDirect(nonCallable, i), TypeError, "calling a non-callable proxy");
        shouldBe(callDirect(native, i), i, "native target");
        let result = callDirect(bound, i);
        shouldBe(result[1], "bound", "bound target first argument");
        shouldBe(result[2], i, "bound target second argument");
    }
}

{
    const error = new Error("thrown");
    const proxy = new Proxy(function () { throw error; }, { });
    for (let i = 0; i < testLoopCount; ++i) {
        let caught;
        try {
            callDirect(proxy, i);
        } catch (e) {
            caught = e;
        }
        shouldBe(caught, error, "exception from the target");
    }
}

function callPolymorphic(callee, a) {
    const result = callee(a);
    return result;
}
noInline(callPolymorphic);

function tailCallPolymorphic(callee, a) {
    return callee(a);
}
noInline(tailCallPolymorphic);

function callMegamorphic(callee, a) {
    const result = callee(a);
    return result;
}
noInline(callMegamorphic);

function tailCallMegamorphic(callee, a) {
    return callee(a);
}
noInline(tailCallMegamorphic);

{
    const functions = [];
    for (let i = 0; i < 4; ++i)
        functions.push(new Function("a", `return a + ${i};`));
    const handler = { };
    const proxy = new Proxy(target, handler);
    const internalFunctionProxy = new Proxy(Array, { });
    for (let i = 0; i < testLoopCount; ++i) {
        for (let j = 0; j < functions.length; ++j) {
            shouldBe(callPolymorphic(functions[j], i), i + j, "polymorphic function");
            shouldBe(tailCallPolymorphic(functions[j], i), i + j, "polymorphic function tail call");
        }
        if (i === (testLoopCount >> 1))
            handler.apply = function () { return "trapped"; };
        let result = callPolymorphic(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "polymorphic proxy before adding the apply trap");
        else
            shouldBe(result, "trapped", "polymorphic proxy after adding the apply trap");
        result = tailCallPolymorphic(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "polymorphic proxy tail call before adding the apply trap");
        else
            shouldBe(result, "trapped", "polymorphic proxy tail call after adding the apply trap");
        shouldBe(callPolymorphic(internalFunctionProxy, 3).length, 3, "polymorphic proxy of an internal function");
        shouldBe(tailCallPolymorphic(internalFunctionProxy, 3).length, 3, "polymorphic proxy tail call of an internal function");
    }
}

{
    const functions = [];
    for (let i = 0; i < 32; ++i)
        functions.push(new Function("a", `return a + ${i};`));
    const handler = { };
    const proxy = new Proxy(target, handler);
    const nested = new Proxy(new Proxy(target, { }), { });
    const internalFunctionProxy = new Proxy(Array, { });
    for (let i = 0; i < testLoopCount; ++i) {
        const j = i % functions.length;
        shouldBe(callMegamorphic(functions[j], i), i + j, "megamorphic function");
        shouldBe(tailCallMegamorphic(functions[j], i), i + j, "megamorphic function tail call");
        if (i === (testLoopCount >> 1))
            handler.apply = function () { return "trapped"; };
        let result = callMegamorphic(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "megamorphic proxy before adding the apply trap");
        else
            shouldBe(result, "trapped", "megamorphic proxy after adding the apply trap");
        result = tailCallMegamorphic(proxy, i);
        if (i < (testLoopCount >> 1))
            shouldBe(result[1], i, "megamorphic proxy tail call before adding the apply trap");
        else
            shouldBe(result, "trapped", "megamorphic proxy tail call after adding the apply trap");
        shouldBe(callMegamorphic(nested, i)[1], i, "megamorphic nested proxy");
        shouldBe(tailCallMegamorphic(nested, i)[1], i, "megamorphic nested proxy tail call");
        shouldBe(callMegamorphic(internalFunctionProxy, 3).length, 3, "megamorphic proxy of an internal function");
        shouldBe(tailCallMegamorphic(internalFunctionProxy, 3).length, 3, "megamorphic proxy tail call of an internal function");
    }
}
