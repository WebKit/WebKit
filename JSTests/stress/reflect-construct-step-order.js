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

const targetMessage = "Reflect.construct requires the first argument be a constructor";
const argumentsMessage = "Reflect.construct requires the second argument be an object";
const newTargetMessage = "Reflect.construct requires the third argument be a constructor if present";

let log = [];

function Recorder()
{
    log.push("construct");
    this.args = Array.prototype.slice.call(arguments);
    this.newTarget = new.target;
}

function Other() { }

function loggingList()
{
    return {
        get length() { log.push("length"); return 2; },
        get 0() { log.push("0"); return "a"; },
        get 1() { log.push("1"); return "b"; },
    };
}

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

// 1. IsConstructor(target), 2-3. IsConstructor(newTarget), 4. CreateListFromArrayLike, 5. Construct.
(function validationOrder() {
    function construct(target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(construct);

    function constructWithoutNewTarget(target, args)
    {
        return Reflect.construct(target, args);
    }
    noInline(constructWithoutNewTarget);

    function check()
    {
        log = [];
        let error = thrownBy(() => construct(() => { }, loggingList(), Other));
        shouldBe(error instanceof TypeError, true);
        shouldBe(error.message, targetMessage);
        shouldBe(log.join(), "");

        error = thrownBy(() => constructWithoutNewTarget(() => { }, loggingList()));
        shouldBe(error.message, targetMessage);
        shouldBe(log.join(), "");

        error = thrownBy(() => construct(Recorder, loggingList(), () => { }));
        shouldBe(error instanceof TypeError, true);
        shouldBe(error.message, newTargetMessage);
        shouldBe(log.join(), "");

        // Both are bad: the target is reported.
        error = thrownBy(() => construct(1, 2, 3));
        shouldBe(error.message, targetMessage);

        // A bad newTarget wins over a bad list.
        error = thrownBy(() => construct(Recorder, 2, 3));
        shouldBe(error.message, newTargetMessage);

        error = thrownBy(() => construct(Recorder, "ab", Other));
        shouldBe(error.message, argumentsMessage);
        shouldBe(log.join(), "");

        let object = construct(Recorder, loggingList(), Other);
        shouldBe(log.join(), "length,0,1,construct");
        shouldBe(object.args.join(), "a,b");
    }

    check();
    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct(Recorder, [i], Other).args[0], i);
        shouldBe(constructWithoutNewTarget(Recorder, [i]).args[0], i);
    }
    check();
    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct(Recorder, [i], Other).args[0], i);
        shouldBe(constructWithoutNewTarget(Recorder, [i]).args[0], i);
    }
    check();
})();

// An explicit undefined newTarget is present, so it is validated. A missing one is not.
(function presentButUndefinedNewTarget() {
    function explicitUndefined(x)
    {
        return Reflect.construct(Recorder, [x], undefined);
    }
    noInline(explicitUndefined);

    function explicitNull(x)
    {
        return Reflect.construct(Recorder, [x], null);
    }
    noInline(explicitNull);

    function missing(x)
    {
        return Reflect.construct(Recorder, [x]);
    }
    noInline(missing);

    function missingList()
    {
        return Reflect.construct(Recorder);
    }
    noInline(missingList);

    function noArguments()
    {
        return Reflect.construct();
    }
    noInline(noArguments);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(thrownBy(() => explicitUndefined(i)).message, newTargetMessage);
        shouldBe(thrownBy(() => explicitNull(i)).message, newTargetMessage);
        shouldBe(log.join(), "");

        let object = missing(i);
        shouldBe(object.newTarget, Recorder);
        shouldBe(object.args[0], i);

        if (i % rareStride)
            continue;
        log = [];
        shouldBe(thrownBy(missingList).message, argumentsMessage);
        shouldBe(thrownBy(noArguments).message, targetMessage);
        shouldBe(log.join(), "");
    }
})();

// A revoked proxy still has [[Construct]], so IsConstructor is true and the list is read before anything throws.
(function revokedProxies() {
    function construct(target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(construct);

    let revokedTarget = Proxy.revocable(Recorder, { });
    revokedTarget.revoke();
    let revokedNewTarget = Proxy.revocable(Other, { });
    revokedNewTarget.revoke();

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct(Recorder, [i], Other).args[0], i);
        if (i % rareStride)
            continue;

        log = [];
        let error = thrownBy(() => construct(revokedTarget.proxy, loggingList(), Other));
        shouldBe(error instanceof TypeError, true);
        shouldBe(log.join(), "length,0,1");

        // Get(newTarget, "prototype") throws before the body of a base constructor runs.
        log = [];
        error = thrownBy(() => construct(Recorder, loggingList(), revokedNewTarget.proxy));
        shouldBe(error instanceof TypeError, true);
        shouldBe(log.join(), "length,0,1");
    }
})();

// CreateListFromArrayLike: the list has to be an Object.
(function listMustBeAnObject() {
    function construct(args)
    {
        return Reflect.construct(Recorder, args);
    }
    noInline(construct);

    let notObjects = [undefined, null, true, 0, 1.5, "ab", "", Symbol("s"), 10n];
    let objects = [[], { }, function (a, b) { }, new String("ab"), new Uint8Array(2), new Map, /x/, new Proxy([1, 2], { })];
    let expectedCounts = [0, 0, 2, 2, 2, 0, 0, 2];

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct([i]).args[0], i);
        if (i % rareStride)
            continue;

        for (let value of notObjects) {
            log = [];
            let error = thrownBy(() => construct(value));
            shouldBe(error instanceof TypeError, true);
            shouldBe(error.message, argumentsMessage);
            shouldBe(log.join(), "");
        }
        for (let j = 0; j < objects.length; ++j)
            shouldBe(construct(objects[j]).args.length, expectedCounts[j]);
        shouldBe(construct(new String("ab")).args.join(), "a,b");
    }
})();

// LengthOfArrayLike: Get(list, "length") once, then ToLength.
(function lengthConversion() {
    function construct(args)
    {
        return Reflect.construct(Recorder, args);
    }
    noInline(construct);

    function listWithLength(length)
    {
        return { length, 0: "a", 1: "b", 2: "c" };
    }

    let cases = [
        [-1, 0], [-0, 0], [NaN, 0], [undefined, 0], [null, 0], [false, 0], [true, 1], [-Infinity, 0],
        [2.9, 2], [0.9, 0], ["2", 2], ["0x2", 2], [" 3 ", 3], ["x", 0], [[2], 2], [[], 0],
    ];

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct([i, 2]).args.length, 2);
        if (i % rareStride)
            continue;

        for (let [length, expected] of cases)
            shouldBe(construct(listWithLength(length)).args.length, expected);

        log = [];
        let valueOfCalls = 0;
        let object = construct({
            0: "a",
            1: "b",
            get length()
            {
                log.push("length");
                return { valueOf() { log.push("valueOf"); ++valueOfCalls; return 2; }, toString() { throw new Error("unreachable"); } };
            },
        });
        shouldBe(object.args.join(), "a,b");
        shouldBe(valueOfCalls, 1);
        shouldBe(log.join(), "length,valueOf,construct");

        log = [];
        shouldBe(thrownBy(() => construct({ length: Symbol() })) instanceof TypeError, true);
        shouldBe(thrownBy(() => construct({ length: 1n })) instanceof TypeError, true);
        shouldBe(thrownBy(() => construct({ get length() { throw new RangeError("length"); } })).message, "length");
        shouldBe(thrownBy(() => construct({ length: { valueOf() { throw new EvalError("valueOf"); } } })).message, "valueOf");
        shouldBe(log.join(), "");
    }
})();

// Elements are read with Get in ascending order, once each, with string keys, before the constructor runs.
(function elementReads() {
    function construct(args)
    {
        return Reflect.construct(Recorder, args);
    }
    noInline(construct);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct([i]).args[0], i);
        if (i % rareStride)
            continue;

        log = [];
        let proxy = new Proxy({ length: 3, 0: "a", 2: "c" }, {
            get(target, key, receiver)
            {
                log.push(`get:${typeof key}:${String(key)}:${receiver === proxy}`);
                return Reflect.get(target, key, receiver);
            },
            has() { log.push("has"); return true; },
            getOwnPropertyDescriptor() { log.push("getOwnPropertyDescriptor"); },
            ownKeys() { log.push("ownKeys"); return []; },
        });
        let object = construct(proxy);
        shouldBe(log.join(), "get:string:length:true,get:string:0:true,get:string:1:true,get:string:2:true,construct");
        shouldBe(object.args.length, 3);
        shouldBe(object.args[1], undefined);
        shouldBe(1 in object.args, true);

        // Properties at or beyond length are ignored.
        shouldBe(construct({ length: 1, 0: "a", 1: "b", 5: "f" }).args.join(), "a");

        // An abrupt Get stops the reads, and the constructor never runs.
        log = [];
        let error = thrownBy(() => construct({
            length: 3,
            get 0() { log.push("0"); return 0; },
            get 1() { log.push("1"); throw new EvalError("one"); },
            get 2() { log.push("2"); return 2; },
        }));
        shouldBe(error.message, "one");
        shouldBe(log.join(), "0,1");

        // Holes are read through the prototype chain.
        let holey = [0, , 2];
        Object.setPrototypeOf(holey, { __proto__: Array.prototype, get 1() { log.push("hole"); return "fromPrototype"; } });
        log = [];
        shouldBe(construct(holey).args.join(), "0,fromPrototype,2");
        shouldBe(log.join(), "hole,construct");
    }
})();

// The length is fixed up front. Later Gets see whatever earlier Gets did to the list.
(function listMutatedWhileItIsRead() {
    function construct(args)
    {
        return Reflect.construct(Recorder, args);
    }
    noInline(construct);

    function show(args)
    {
        return JSON.stringify(construct(args).args);
    }

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(construct([i, 1, 2]).args.length, 3);
        if (i % rareStride)
            continue;

        let shrink = [0, 1, 2, 3];
        Object.defineProperty(shrink, 0, { get() { shrink.length = 2; return "g"; } });
        shouldBe(show(shrink), `["g",1,null,null]`);

        let grow = [0, 1];
        Object.defineProperty(grow, 0, { get() { grow.push(9, 9); return "g"; } });
        shouldBe(show(grow), `["g",1]`);

        let overwrite = [0, 1, 2];
        Object.defineProperty(overwrite, 0, { get() { overwrite[2] = "w"; return "g"; } });
        shouldBe(show(overwrite), `["g",1,"w"]`);

        let remove = [0, 1, 2];
        Object.setPrototypeOf(remove, { __proto__: Array.prototype, 2: "proto" });
        Object.defineProperty(remove, 0, { get() { delete remove[2]; return "g"; } });
        shouldBe(show(remove), `["g",1,"proto"]`);

        let retype = [0.5, 1.5, 2.5];
        Object.defineProperty(retype, 1, { get() { retype[0] = "s"; retype[2] = { }; return "g"; } });
        shouldBe(show(retype), `[0.5,"g",{}]`);

        let mapped = (function (x, y, z) {
            Object.defineProperty(arguments, 0, { get() { z = "Z"; arguments.length = 1; return "g"; } });
            return arguments;
        })(1, 2, 3);
        shouldBe(show(mapped), `["g",2,"Z"]`);

        let lengthSideEffect = { 0: "a", 1: "b", get length() { return { valueOf() { lengthSideEffect[1] = "changed"; return 2; } }; } };
        shouldBe(show(lengthSideEffect), `["a","changed"]`);
    }
})();

// The list is a snapshot: what the constructor does to the list afterwards does not change its own arguments.
(function constructorMutatesTheList() {
    let shared = [1, 2, 3];

    function Mutator(a, b, c)
    {
        shared.length = 0;
        shared.push("x");
        this.sum = a + b + c;
        this.count = arguments.length;
    }

    function construct(args)
    {
        return Reflect.construct(Mutator, args);
    }
    noInline(construct);

    function literal(a, b, c)
    {
        let array;
        let object = Reflect.construct(function (x, y, z) { array[0] = "x"; this.sum = x + y + z; }, array = [a, b, c]);
        return object.sum + array[0];
    }
    noInline(literal);

    for (let i = 0; i < testLoopCount; ++i) {
        shared = [i, 2, 3];
        let object = construct(shared);
        shouldBe(object.sum, i + 5);
        shouldBe(object.count, 3);
        shouldBe(shared.join(), "x");
        shouldBe(literal(i, 2, 3), (i + 5) + "x");
    }
})();

// The this value of Reflect.construct is ignored, extra arguments are ignored, and it is not a constructor itself.
(function callForms() {
    function viaCall(x)
    {
        return Reflect.construct.call(x, Recorder, [x], Other);
    }
    noInline(viaCall);

    function viaApply(x)
    {
        return Reflect.construct.apply(null, [Recorder, [x], Other]);
    }
    noInline(viaApply);

    function viaReflectApply(x)
    {
        return Reflect.apply(Reflect.construct, 42, [Recorder, [x]]);
    }
    noInline(viaReflectApply);

    function viaBind(x)
    {
        return boundConstruct([x], Other);
    }
    noInline(viaBind);
    let boundConstruct = Reflect.construct.bind(null, Recorder);

    function extraArguments(x)
    {
        return Reflect.construct(Recorder, [x], Other, () => { }, 5);
    }
    noInline(extraArguments);

    function nested(x)
    {
        return Reflect.construct(Recorder, [Reflect.construct(Recorder, [x], Other)], Reflect.construct(Recorder, [Other]).args[0]);
    }
    noInline(nested);

    function nestedFunctionConstructor(x)
    {
        return Reflect.construct(Recorder, [x], Reflect.construct(Function, ["this.nested = true"]));
    }
    noInline(nestedFunctionConstructor);

    function asConstructor(x)
    {
        return new Reflect.construct(Recorder, [x]);
    }
    noInline(asConstructor);

    function selfTarget(x)
    {
        return Reflect.construct(Reflect.construct, [Recorder, [x]]);
    }
    noInline(selfTarget);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        for (let func of [viaCall, viaApply, viaBind, extraArguments]) {
            let object = func(i);
            shouldBe(object.args[0], i);
            shouldBe(object.newTarget, Other);
            shouldBe(Object.getPrototypeOf(object), Other.prototype);
        }
        let object = viaReflectApply(i);
        shouldBe(object.args[0], i);
        shouldBe(object.newTarget, Recorder);

        object = nested(i);
        shouldBe(object.args[0].args[0], i);
        shouldBe(object.args[0].newTarget, Other);
        shouldBe(object.newTarget, Other);

        if (i % rareStride)
            continue;
        object = nestedFunctionConstructor(i);
        shouldBe(typeof object.newTarget, "function");
        shouldBe(object.nested, undefined);
        shouldBe(Object.getPrototypeOf(object), object.newTarget.prototype);
        shouldBe(thrownBy(() => asConstructor(i)) instanceof TypeError, true);
        shouldBe(thrownBy(() => selfTarget(i)).message, targetMessage);
    }
})();
