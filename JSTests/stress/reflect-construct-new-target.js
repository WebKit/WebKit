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

let log = [];

function Base(x)
{
    log.push("body");
    this.x = x;
    this.newTarget = new.target;
}

class ClassBase {
    constructor(x)
    {
        log.push("body");
        this.x = x;
        this.newTarget = new.target;
    }
}

class Derived extends ClassBase {
    constructor(x)
    {
        log.push("derived");
        super(x);
        this.derivedNewTarget = new.target;
    }
}

function Other() { }

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

// Construct(target, list, newTarget): new.target is newTarget, and a base constructor's this comes from
// Get(newTarget, "prototype"), read once, after the list and before the body.
(function prototypeIsReadFromNewTarget() {
    function literal(x, newTarget)
    {
        return Reflect.construct(Base, [x], newTarget);
    }
    noInline(literal);

    function generic(target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(generic);

    function literalDerived(x, newTarget)
    {
        return Reflect.construct(Derived, [x], newTarget);
    }
    noInline(literalDerived);

    let proxyPrototype = { proxy: true };
    let proxyNewTarget = new Proxy(function () { }, {
        get(target, key, receiver)
        {
            log.push("get:" + String(key));
            return key === "prototype" ? proxyPrototype : Reflect.get(target, key, receiver);
        }
    });

    let boundPrototype = { bound: true };
    let boundNewTarget = function () { }.bind();
    Object.defineProperty(boundNewTarget, "prototype", { get() { log.push("boundPrototype"); return boundPrototype; } });

    let list = {
        get length() { log.push("length"); return 1; },
        get 0() { log.push("0"); return "zero"; },
    };

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        let object = literal(i, Other);
        shouldBe(object.x, i);
        shouldBe(object.newTarget, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);
        shouldBe(object instanceof Base, false);

        object = literalDerived(i, Other);
        shouldBe(object.x, i);
        shouldBe(object.newTarget, Other);
        shouldBe(object.derivedNewTarget, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);

        log = [];
        object = literal(i, proxyNewTarget);
        shouldBe(log.join(), "get:prototype,body");
        shouldBe(object.newTarget, proxyNewTarget);
        shouldBe(Object.getPrototypeOf(object), proxyPrototype);

        log = [];
        object = literal(i, boundNewTarget);
        shouldBe(log.join(), "boundPrototype,body");
        shouldBe(object.newTarget, boundNewTarget);
        shouldBe(Object.getPrototypeOf(object), boundPrototype);

        if (i % 10)
            continue;

        log = [];
        object = generic(Base, list, proxyNewTarget);
        shouldBe(log.join(), "length,0,get:prototype,body");
        shouldBe(object.x, "zero");
        shouldBe(Object.getPrototypeOf(object), proxyPrototype);

        // For a derived constructor the read happens when the base constructor is reached.
        log = [];
        object = generic(Derived, list, proxyNewTarget);
        shouldBe(log.join(), "length,0,derived,get:prototype,body");
        shouldBe(Object.getPrototypeOf(object), proxyPrototype);

        log = [];
        object = literalDerived(i, boundNewTarget);
        shouldBe(log.join(), "derived,boundPrototype,body");
        shouldBe(Object.getPrototypeOf(object), boundPrototype);
    }
})();

// If newTarget.prototype is not an Object, the default comes from GetFunctionRealm(newTarget), not from the target
// and not from the caller.
(function nonObjectPrototypeUsesTheRealmOfNewTarget() {
    const other = createGlobalObject();

    function literal(x, newTarget)
    {
        return Reflect.construct(Base, [x], newTarget);
    }
    noInline(literal);

    function generic(target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(generic);

    function literalArray(length, newTarget)
    {
        return Reflect.construct(Array, [length], newTarget);
    }
    noInline(literalArray);

    function withPrototype(func, prototype)
    {
        func.prototype = prototype;
        return func;
    }

    let local = withPrototype(function () { }, null);
    let foreign = withPrototype(other.Function("return function () { }")(), 1);
    let foreignBound = other.Function("return function () { }.bind()")();
    let foreignProxy = new Proxy(withPrototype(other.Function("return function () { }")(), "string"), { });
    let foreignThroughLocalBound = Function.prototype.bind.call(withPrototype(other.Function("return function () { }")(), undefined));

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(Object.getPrototypeOf(literal(i, local)), Object.prototype);
        shouldBe(Object.getPrototypeOf(literal(i, foreign)), other.Object.prototype);
        shouldBe(Object.getPrototypeOf(literalArray(i & 7, foreign)), other.Array.prototype);
        if (i % 10)
            continue;

        shouldBe(Object.getPrototypeOf(literal(i, foreignBound)), other.Object.prototype);
        shouldBe(Object.getPrototypeOf(literal(i, foreignProxy)), other.Object.prototype);
        shouldBe(Object.getPrototypeOf(literal(i, foreignThroughLocalBound)), other.Object.prototype);
        shouldBe(Object.getPrototypeOf(generic(Base, [i], foreign)), other.Object.prototype);
        shouldBe(Object.getPrototypeOf(generic(Map, [], foreign)), other.Map.prototype);
        shouldBe(Object.getPrototypeOf(generic(other.Array, [1], local)), Array.prototype);
    }
})();

// An abrupt Get(newTarget, "prototype") propagates, and the body of a base constructor does not run.
(function prototypeGetterThrows() {
    function literal(x, newTarget)
    {
        return Reflect.construct(Base, [x], newTarget);
    }
    noInline(literal);

    function literalDerived(x, newTarget)
    {
        return Reflect.construct(Derived, [x], newTarget);
    }
    noInline(literalDerived);

    let thrower = function () { }.bind();
    Object.defineProperty(thrower, "prototype", { get() { log.push("prototype"); throw new EvalError("prototype"); } });

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(literal(i, Other).x, i);
        shouldBe(literalDerived(i, Other).x, i);
        if (i % rareStride)
            continue;

        log = [];
        shouldBe(thrownBy(() => literal(i, thrower)).message, "prototype");
        shouldBe(log.join(), "prototype");

        log = [];
        shouldBe(thrownBy(() => literalDerived(i, thrower)).message, "prototype");
        shouldBe(log.join(), "derived,prototype");
    }
})();

// The result rules of [[Construct]] for base and derived constructors.
(function returnValues() {
    let returned = { returned: true };

    function BaseReturning(value) { this.own = true; return value; }

    class DerivedReturning extends ClassBase {
        constructor(value, callSuper)
        {
            if (callSuper)
                super(0);
            return value;
        }
    }

    function literalBase(value)
    {
        return Reflect.construct(BaseReturning, [value]);
    }
    noInline(literalBase);

    function literalDerivedReturning(value, callSuper)
    {
        return Reflect.construct(DerivedReturning, [value, callSuper], Other);
    }
    noInline(literalDerivedReturning);

    function genericDerivedReturning(args)
    {
        return Reflect.construct(DerivedReturning, args, Other);
    }
    noInline(genericDerivedReturning);

    let primitives = [1, "s", true, null, Symbol(), 1n];

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(literalBase(returned), returned);
        shouldBe(literalBase(i).own, true);
        shouldBe(literalBase(undefined).own, true);

        shouldBe(literalDerivedReturning(returned, false), returned);
        shouldBe(literalDerivedReturning(returned, true), returned);
        shouldBe(Object.getPrototypeOf(literalDerivedReturning(undefined, true)), Other.prototype);
        shouldBe(genericDerivedReturning([returned, false]), returned);

        if (i % rareStride)
            continue;
        for (let primitive of primitives) {
            shouldBe(literalBase(primitive).own, true);
            shouldBe(thrownBy(() => literalDerivedReturning(primitive, true)) instanceof TypeError, true);
            shouldBe(thrownBy(() => literalDerivedReturning(primitive, false)) instanceof TypeError, true);
            shouldBe(thrownBy(() => genericDerivedReturning([primitive, true])) instanceof TypeError, true);
        }
        shouldBe(thrownBy(() => literalDerivedReturning(undefined, false)) instanceof ReferenceError, true);
        shouldBe(thrownBy(() => genericDerivedReturning([undefined, false])) instanceof ReferenceError, true);
    }
})();

// Class constructors cannot be called, but they are constructors. Functions without [[Construct]] are rejected even
// though they are callable.
(function whatIsAConstructor() {
    function generic(target, args)
    {
        return Reflect.construct(target, args);
    }
    noInline(generic);

    function genericNewTarget(newTarget)
    {
        return Reflect.construct(Base, [1], newTarget);
    }
    noInline(genericNewTarget);

    let constructors = [
        Base, ClassBase, Derived, class { }, function () { }, Base.bind(null), ClassBase.bind(null), new Proxy(Base, { }), new Proxy(ClassBase, { }),
        Object, Array, Function, Date, Map, Promise.bind(null), Proxy.revocable(Base, { }).proxy,
    ];
    let notConstructors = [
        () => { }, async () => { }, async function () { }, function* () { }, async function* () { }, { method() { } }.method,
        Object.getOwnPropertyDescriptor({ get accessor() { return 1; } }, "accessor").get, class { static method() { } }.method,
        (() => { }).bind(null), new Proxy(() => { }, { }), new Proxy({ }, { }), Math.max, Reflect.construct, Function.prototype, parseInt,
        Base.prototype, { }, [], Symbol.iterator,
    ];

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(generic(Base, [i]).x, i);
        shouldBe(genericNewTarget(Other).x, 1);
        if (i % (5 * rareStride))
            continue;

        for (let constructor of constructors) {
            if (constructor === Promise || constructor.name === "bound Promise")
                continue;
            let result = generic(constructor, []);
            shouldBe(Object(result), result);
            shouldBe(genericNewTarget(constructor).newTarget, constructor);
        }
        for (let notConstructor of notConstructors) {
            shouldBe(thrownBy(() => generic(notConstructor, [])) instanceof TypeError, true);
            shouldBe(thrownBy(() => genericNewTarget(notConstructor)) instanceof TypeError, true);
        }
    }
})();

// Bound function [[Construct]]: bound arguments come first, and newTarget is replaced by the bound target only when
// it is the bound function itself.
(function boundTargets() {
    let bound = Base.bind({ ignored: true }, "bound");
    let boundTwice = bound.bind(null, "ignored");
    let boundClass = ClassBase.bind(null);

    function literal(target, x, newTarget)
    {
        return Reflect.construct(target, [x], newTarget);
    }
    noInline(literal);

    function literalWithoutNewTarget(target, x)
    {
        return Reflect.construct(target, [x]);
    }
    noInline(literalWithoutNewTarget);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        let object = literalWithoutNewTarget(bound, i);
        shouldBe(object.x, "bound");
        shouldBe(object.newTarget, Base);
        shouldBe(Object.getPrototypeOf(object), Base.prototype);
        shouldBe(object.ignored, undefined);

        object = literal(bound, i, bound);
        shouldBe(object.newTarget, Base);

        object = literal(bound, i, Other);
        shouldBe(object.x, "bound");
        shouldBe(object.newTarget, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);

        // newTarget is a different bound function, so it is passed through unchanged.
        object = literal(bound, i, boundTwice);
        shouldBe(object.newTarget, boundTwice);
        shouldBe(Object.getPrototypeOf(object), Object.prototype);

        object = literalWithoutNewTarget(boundTwice, i);
        shouldBe(object.x, "bound");
        shouldBe(object.newTarget, Base);

        object = literal(boundClass, i, boundClass);
        shouldBe(object.x, i);
        shouldBe(object.newTarget, ClassBase);
    }
})();

// Proxy [[Construct]]: the trap gets the target, a fresh Array and newTarget; a non-Object result is a TypeError;
// without a trap, newTarget is forwarded to the target.
(function proxyTargets() {
    let seen;
    let trapped = new Proxy(Base, {
        construct(target, args, newTarget)
        {
            seen = { target, args, newTarget, handler: this };
            return Reflect.construct(target, args, newTarget);
        }
    });
    let forwarding = new Proxy(ClassBase, { });
    let returnsPrimitive = new Proxy(Base, { construct() { return 1; } });
    let returnsOther = new Proxy(Base, { construct() { return Other; } });
    let nested = new Proxy(trapped, { });

    function literal(target, x, newTarget)
    {
        return Reflect.construct(target, [x], newTarget);
    }
    noInline(literal);

    function generic(target, args)
    {
        return Reflect.construct(target, args);
    }
    noInline(generic);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        let list = [i, "extra"];
        let object = generic(trapped, list);
        shouldBe(object.x, i);
        shouldBe(object.newTarget, trapped);
        shouldBe(seen.target, Base);
        shouldBe(seen.newTarget, trapped);
        shouldBe(Array.isArray(seen.args), true);
        shouldBe(seen.args === list, false);
        shouldBe(seen.args.join(), i + ",extra");
        shouldBe(Object.getPrototypeOf(seen.args), Array.prototype);

        object = literal(trapped, i, Other);
        shouldBe(seen.newTarget, Other);
        shouldBe(Object.getPrototypeOf(object), Other.prototype);

        object = literal(forwarding, i, Other);
        shouldBe(object.x, i);
        shouldBe(object.newTarget, Other);

        object = literal(nested, i, nested);
        shouldBe(seen.newTarget, nested);
        shouldBe(object.newTarget, nested);

        shouldBe(literal(returnsOther, i, Other), Other);

        if (i % rareStride)
            continue;
        shouldBe(thrownBy(() => literal(returnsPrimitive, i, Other)) instanceof TypeError, true);

        // The trap is looked up after the list has been read.
        log = [];
        let lookedUp = new Proxy(Base, {
            get construct() { log.push("trap"); return undefined; }
        });
        generic(lookedUp, { get length() { log.push("length"); return 0; } });
        shouldBe(log.join(), "length,trap,body");
    }
})();
