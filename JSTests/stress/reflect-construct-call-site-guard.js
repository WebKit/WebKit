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

function Point(x, y)
{
    this.x = x;
    this.y = y;
    this.count = arguments.length;
    this.newTarget = new.target;
}

function Other() { }

let log = [];

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

// The call site only takes the fast path while the callee is this realm's original Reflect.construct.
(function calleeIsNotTheOriginalReflectConstruct() {
    const other = createGlobalObject();

    function literal(Reflect, x, y)
    {
        return Reflect.construct(Point, [x, y]);
    }
    noInline(literal);

    function generic(Reflect, target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(generic);

    let received;
    let fake = {
        construct(target, args, newTarget)
        {
            received = { self: this, target, args, newTarget, count: arguments.length };
            return "fake";
        }
    };

    let getterLog = [];
    let withGetter = {
        get construct()
        {
            getterLog.push("get");
            return Reflect.construct;
        }
    };

    for (let i = 0; i < testLoopCount; ++i) {
        let point = literal(Reflect, i, 2);
        shouldBe(point.x, i);
        shouldBe(point.count, 2);
        shouldBe(point instanceof Point, true);
        shouldBe(generic(Reflect, Point, [i], Other).newTarget, Other);

        // Another realm's Reflect.construct builds the same object, but reports its own errors from its own realm.
        point = literal(other.Reflect, i, 2);
        shouldBe(point.x, i);
        shouldBe(point instanceof Point, true);
        point = generic(other.Reflect, Point, [i, 2], Other);
        shouldBe(point.count, 2);
        shouldBe(Object.getPrototypeOf(point), Other.prototype);

        shouldBe(literal(fake, i, 2), "fake");
        shouldBe(received.self, fake);
        shouldBe(received.target, Point);
        shouldBe(Array.isArray(received.args), true);
        shouldBe(received.args.join(), i + ",2");
        shouldBe(received.count, 2);
        shouldBe(generic(fake, Point, "not an object", 5), "fake");
        shouldBe(received.args, "not an object");
        shouldBe(received.newTarget, 5);
        shouldBe(received.count, 3);

        getterLog = [];
        shouldBe(literal(withGetter, i, 2).x, i);
        shouldBe(getterLog.join(), "get");

        if (i % rareStride)
            continue;
        shouldBe(thrownBy(() => generic(Reflect, () => { }, [], Other)) instanceof TypeError, true);
        shouldBe(thrownBy(() => generic(other.Reflect, () => { }, [], Other)) instanceof other.TypeError, true);
        shouldBe(thrownBy(() => generic(Reflect, Point, 1, Other)) instanceof TypeError, true);
        shouldBe(thrownBy(() => generic(other.Reflect, Point, 1, Other)) instanceof other.TypeError, true);
        shouldBe(thrownBy(() => literal(null, i, 2)) instanceof TypeError, true);
        shouldBe(thrownBy(() => literal({ }, i, 2)) instanceof TypeError, true);
    }
})();

(function replacedOnTheGlobalReflect() {
    function literal(x)
    {
        return Reflect.construct(Point, [x]);
    }
    noInline(literal);

    function generic(args)
    {
        return Reflect.construct(Point, args);
    }
    noInline(generic);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(literal(i).x, i);
        shouldBe(generic([i]).x, i);
    }

    let original = Reflect.construct;
    Reflect.construct = function (target, args) { return "replaced:" + args.length; };
    shouldBe(literal(1), "replaced:1");
    shouldBe(generic([1, 2]), "replaced:2");
    Reflect.construct = original;
    shouldBe(literal(1).x, 1);
    shouldBe(generic([1]).x, 1);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(literal(i).x, i);
        shouldBe(generic([i]).x, i);
    }
})();

(function optionalChaining() {
    function optionalBase(Reflect, x)
    {
        return Reflect?.construct(Point, [log.push("element"), x]);
    }
    noInline(optionalBase);

    function optionalCall(Reflect, x)
    {
        return Reflect.construct?.(Point, [log.push("element"), x]);
    }
    noInline(optionalCall);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        shouldBe(optionalBase(Reflect, i).y, i);
        shouldBe(optionalCall(Reflect, i).y, i);
        shouldBe(log.join(), "element,element");

        log = [];
        shouldBe(optionalBase(undefined, i), undefined);
        shouldBe(optionalBase(null, i), undefined);
        shouldBe(optionalCall({ }, i), undefined);
        shouldBe(optionalCall({ construct: null }, i), undefined);
        shouldBe(log.join(), "");
    }
})();

// Every argument expression is evaluated once, left to right, before any validation.
(function argumentEvaluation() {
    function record(name, value)
    {
        log.push(name);
        return value;
    }

    function literal(x)
    {
        return Reflect.construct(record("target", Point), [record("element0", x), record("element1", 1)], record("newTarget", Other), record("extra0", 0), record("extra1", 1));
    }
    noInline(literal);

    function generic(x)
    {
        return Reflect.construct(record("target", Point), record("list", [x]), record("newTarget", Other), record("extra", 0));
    }
    noInline(generic);

    function invalid(target, newTarget)
    {
        return Reflect.construct(record("target", target), [record("element", 1)], record("newTarget", newTarget), record("extra", 0));
    }
    noInline(invalid);

    function assignsTarget(x)
    {
        let target = Point;
        let object = Reflect.construct(target, [target = Other, x]);
        return [object, target];
    }
    noInline(assignsTarget);

    function assignsList(x)
    {
        let list = [x, 1];
        let newTarget = Other;
        let object = Reflect.construct(Point, list, (list = [x, 2, 3], newTarget), newTarget = Point);
        return [object, list, newTarget];
    }
    noInline(assignsList);

    for (let i = 0; i < testLoopCount; ++i) {
        log = [];
        let point = literal(i);
        shouldBe(log.join(), "target,element0,element1,newTarget,extra0,extra1");
        shouldBe(point.x, i);
        shouldBe(point.count, 2);
        shouldBe(point.newTarget, Other);

        log = [];
        point = generic(i);
        shouldBe(log.join(), "target,list,newTarget,extra");
        shouldBe(point.x, i);
        shouldBe(point.newTarget, Other);

        let [object, target] = assignsTarget(i);
        shouldBe(object instanceof Point, true);
        shouldBe(object.x, Other);
        shouldBe(object.y, i);
        shouldBe(target, Other);

        let [object2, list, newTarget] = assignsList(i);
        shouldBe(object2.count, 2);
        shouldBe(object2.y, 1);
        shouldBe(object2.newTarget, Other);
        shouldBe(list.length, 3);
        shouldBe(newTarget, Point);

        if (i % rareStride)
            continue;
        log = [];
        shouldBe(thrownBy(() => invalid(() => { }, Other)).message, "Reflect.construct requires the first argument be a constructor");
        shouldBe(log.join(), "target,element,newTarget,extra");
        log = [];
        shouldBe(thrownBy(() => invalid(Point, () => { })).message, "Reflect.construct requires the third argument be a constructor if present");
        shouldBe(log.join(), "target,element,newTarget,extra");
    }
})();

// These spellings are ordinary calls of Reflect.construct.
(function ordinaryCalls() {
    function spreadAll(args)
    {
        return Reflect.construct(...args);
    }
    noInline(spreadAll);

    function spreadTail(target, rest)
    {
        return Reflect.construct(target, ...rest);
    }
    noInline(spreadTail);

    function oneArgument(target)
    {
        return Reflect.construct(target);
    }
    noInline(oneArgument);

    function bracket(x)
    {
        return Reflect["construct"](Point, [x]);
    }
    noInline(bracket);

    function throughGlobalThis(x)
    {
        return globalThis.Reflect.construct(Point, [x]);
    }
    noInline(throughGlobalThis);

    function holeyLiteral(x)
    {
        return Reflect.construct(Point, [x, , 3]);
    }
    noInline(holeyLiteral);

    function spreadLiteral(x, rest)
    {
        return Reflect.construct(Point, [x, ...rest]);
    }
    noInline(spreadLiteral);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(spreadAll([Point, [i, 2]]).x, i);
        shouldBe(spreadAll([Point, [i], Other]).newTarget, Other);
        shouldBe(spreadTail(Point, [[i, 2], Other]).count, 2);
        shouldBe(bracket(i).x, i);
        shouldBe(throughGlobalThis(i).x, i);

        let point = holeyLiteral(i);
        shouldBe(point.count, 3);
        shouldBe(point.y, undefined);

        point = spreadLiteral(i, [2, 3]);
        shouldBe(point.count, 3);
        shouldBe(point.y, 2);

        if (i % rareStride)
            continue;
        shouldBe(thrownBy(() => oneArgument(Point)).message, "Reflect.construct requires the second argument be an object");
        shouldBe(thrownBy(() => spreadAll([])).message, "Reflect.construct requires the first argument be a constructor");
    }
})();

(function nesting() {
    class Box {
        constructor(value)
        {
            this.value = value;
        }
    }

    function nested(x)
    {
        return Reflect.construct(Box, [Reflect.construct(Box, [Reflect.construct(Box, [Reflect.construct(Box, [Reflect.construct(Box, [x])])])])]);
    }
    noInline(nested);

    function nestedWithApply(x)
    {
        return Reflect.construct(Box, [Math.max.apply(null, [Reflect.construct(Box, [Math.max.call(null, Reflect.construct(Box, [x]).value, 0)]).value, 0])]);
    }
    noInline(nestedWithApply);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(nested(i).value.value.value.value.value, i);
        shouldBe(nestedWithApply(i).value, i);
    }
})();

(function positions() {
    "use strict";

    let count = 0;
    class Counter {
        constructor(step)
        {
            count += step;
        }
    }

    function tail(x)
    {
        return Reflect.construct(Point, [x, 1]);
    }
    noInline(tail);

    function ignoredResult(step)
    {
        Reflect.construct(Counter, [step]);
    }
    noInline(ignoredResult);

    function inCondition(args)
    {
        return Reflect.construct(Point, args) ? "object" : "unreachable";
    }
    noInline(inCondition);

    class Derived extends Point {
        constructor(...args)
        {
            super(...args);
            this.inner = Reflect.construct(Point, args, new.target);
            this.arrow = (() => Reflect.construct(Point, [this.x], new.target))();
        }
    }

    function* generator(x)
    {
        yield Reflect.construct(Point, [x, yield "first"]);
    }

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(tail(i).x, i);
        ignoredResult(1);
        shouldBe(inCondition([i]), "object");

        let derived = new Derived(i, 2);
        shouldBe(derived.inner.y, 2);
        shouldBe(derived.inner.newTarget, Derived);
        shouldBe(derived.arrow.x, i);
        shouldBe(derived.arrow.newTarget, Derived);

        if (i % rareStride)
            continue;
        let iterator = generator(i);
        shouldBe(iterator.next().value, "first");
        let point = iterator.next("sent").value;
        shouldBe(point.x, i);
        shouldBe(point.y, "sent");
    }
    shouldBe(count, testLoopCount);
})();

(function withStatement() {
    let fake = { construct() { return "fake"; } };

    function test(scope, x)
    {
        with (scope)
            return Reflect.construct(Point, [x]);
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(test({ }, i).x, i);
        shouldBe(test({ Reflect: fake }, i), "fake");
        shouldBe(test({ Point: Other }, i) instanceof Other, true);
    }
})();
