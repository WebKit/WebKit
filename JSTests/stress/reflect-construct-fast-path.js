function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrow(func, errorConstructor, message)
{
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorConstructor))
        throw new Error(`bad error: ${String(error)}`);
    if (message !== undefined && error.message !== message)
        throw new Error(`bad error message: ${error.message}`);
}

class Point {
    constructor(x, y)
    {
        this.x = x;
        this.y = y;
        this.count = arguments.length;
        this.newTarget = new.target;
    }
}

class Point3D extends Point {
    constructor(x, y, z)
    {
        super(x, y);
        this.z = z;
    }
}

function Other() { }

const targetMessage = "Reflect.construct requires the first argument be a constructor";
const argumentsMessage = "Reflect.construct requires the second argument be an object";
const newTargetMessage = "Reflect.construct requires the third argument be a constructor if present";

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

(function arrayLiteral() {
    function test(x, y)
    {
        return Reflect.construct(Point, [x, y]);
    }
    noInline(test);

    function testWithNewTarget(x, y, newTarget)
    {
        return Reflect.construct(Point, [x, y], newTarget);
    }
    noInline(testWithNewTarget);

    function testEmpty()
    {
        return Reflect.construct(Point, []);
    }
    noInline(testEmpty);

    function testDerived(x, y, z)
    {
        return Reflect.construct(Point3D, [x, y, z]);
    }
    noInline(testDerived);

    function testExtraArguments(x, y)
    {
        return Reflect.construct(Point, [x, y], Other, 42);
    }
    noInline(testExtraArguments);

    for (let i = 0; i < testLoopCount; ++i) {
        let point = test(i, "y");
        shouldBe(point.x, i);
        shouldBe(point.y, "y");
        shouldBe(point.count, 2);
        shouldBe(point.newTarget, Point);
        shouldBe(Object.getPrototypeOf(point), Point.prototype);

        point = testWithNewTarget(i, 1.5, i & 1 ? Other : Point3D);
        shouldBe(point.x, i);
        shouldBe(point.y, 1.5);
        shouldBe(point.newTarget, i & 1 ? Other : Point3D);
        shouldBe(Object.getPrototypeOf(point), i & 1 ? Other.prototype : Point3D.prototype);

        point = testEmpty();
        shouldBe(point.x, undefined);
        shouldBe(point.count, 0);

        point = testDerived(i, 2, 3);
        shouldBe(point.x, i);
        shouldBe(point.z, 3);
        shouldBe(point.newTarget, Point3D);
        shouldBe(Object.getPrototypeOf(point), Point3D.prototype);

        point = testExtraArguments(i, 2);
        shouldBe(point.x, i);
        shouldBe(Object.getPrototypeOf(point), Other.prototype);
    }
})();

(function listModifiedByLaterArgument() {
    function test(x, y)
    {
        let array;
        return Reflect.construct(Point, array = [x, y], (array[1] = "stored", array.length = 3, Point));
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        let point = test(i, 2);
        shouldBe(point.x, i);
        shouldBe(point.y, "stored");
        shouldBe(point.count, 3);
    }
})();

(function argumentsObjects() {
    function Base(x, y)
    {
        this.x = x;
        this.y = y;
        this.count = arguments.length;
    }

    function Sloppy()
    {
        return Reflect.construct(Base, arguments, new.target);
    }
    Sloppy.prototype = Object.create(Base.prototype);

    function Strict()
    {
        "use strict";
        return Reflect.construct(Base, arguments, new.target);
    }
    Strict.prototype = Object.create(Base.prototype);

    function Mapped(x, y)
    {
        x = "mapped";
        return Reflect.construct(Base, arguments, new.target);
    }

    function OverriddenLength()
    {
        arguments.length = 1;
        return Reflect.construct(Base, arguments, new.target);
    }

    function rest(constructor, ...args)
    {
        return Reflect.construct(constructor, args);
    }
    noInline(rest);

    function spread(args)
    {
        return Reflect.construct(Base, [0, ...args]);
    }
    noInline(spread);

    function test(constructor, x, y)
    {
        return new constructor(x, y);
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        let object = test(Sloppy, i, 2);
        shouldBe(object.x, i);
        shouldBe(object.y, 2);
        shouldBe(object.count, 2);
        shouldBe(Object.getPrototypeOf(object), Sloppy.prototype);

        object = test(Strict, i, 2);
        shouldBe(object.x, i);
        shouldBe(object.count, 2);
        shouldBe(Object.getPrototypeOf(object), Strict.prototype);

        object = test(Mapped, i, 2);
        shouldBe(object.x, "mapped");
        shouldBe(object.y, 2);

        object = test(OverriddenLength, i, 2);
        shouldBe(object.x, i);
        shouldBe(object.y, undefined);
        shouldBe(object.count, 1);

        object = rest(Base, i, 2, 3);
        shouldBe(object.x, i);
        shouldBe(object.count, 3);

        object = rest(Base);
        shouldBe(object.count, 0);

        object = spread([i, 2]);
        shouldBe(object.x, 0);
        shouldBe(object.y, i);
        shouldBe(object.count, 3);
    }
})();

(function tooManyArguments() {
    function construct(target, args)
    {
        return Reflect.construct(target, args);
    }
    noInline(construct);

    let touched = false;
    let tooLong = {
        length: 0x100001,
        get 0() { touched = true; return 0; },
    };
    function strictArgumentsWithLongLength()
    {
        "use strict";
        arguments.length = 2 ** 32 + 1;
        return arguments;
    }

    for (let i = 0; i < testLoopCount; ++i) {
        construct(Point, [i]);
        if (!(i % (10 * rareStride))) {
            shouldThrow(() => construct(Point, tooLong), RangeError, "Maximum call stack size exceeded.");
            shouldBe(touched, false);
            shouldThrow(() => construct(Point, strictArgumentsWithLongLength(1, 2, 3)), RangeError, "Maximum call stack size exceeded.");
        }
    }
})();

(function errors() {
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

    function constructLiteral(target, x, newTarget)
    {
        return Reflect.construct(target, [x], newTarget);
    }
    noInline(constructLiteral);

    let notConstructors = [undefined, null, 1, "string", { }, () => { }, { method() { } }.method, async function () { }, function* () { }, Math.max, Symbol()];
    let notObjects = [undefined, null, 1, "string", true, Symbol(), 1n];

    function checkAll()
    {
        for (let value of notConstructors) {
            shouldThrow(() => construct(value, [], Point), TypeError, targetMessage);
            shouldThrow(() => constructWithoutNewTarget(value, []), TypeError, targetMessage);
            shouldThrow(() => constructLiteral(value, 1, Point), TypeError, targetMessage);
            shouldThrow(() => construct(Point, [], value), TypeError, newTargetMessage);
            shouldThrow(() => constructLiteral(Point, 1, value), TypeError, newTargetMessage);
        }
        for (let value of notObjects) {
            shouldThrow(() => construct(Point, value, Point), TypeError, argumentsMessage);
            shouldThrow(() => constructWithoutNewTarget(Point, value), TypeError, argumentsMessage);
        }
        shouldThrow(() => construct(undefined, undefined, undefined), TypeError, targetMessage);
        shouldThrow(() => construct(Point, undefined, undefined), TypeError, newTargetMessage);
    }

    function warmUp()
    {
        for (let i = 0; i < testLoopCount; ++i) {
            shouldBe(construct(Point, [i], Point).x, i);
            shouldBe(constructWithoutNewTarget(Point, [i]).x, i);
            shouldBe(constructLiteral(Point, i, Point).x, i);
        }
    }

    checkAll();
    warmUp();
    checkAll();
    warmUp();
    checkAll();
})();

(function constructorThatThrowsFromConstruct() {
    function test(x)
    {
        return Reflect.construct(Symbol, [x]);
    }
    noInline(test);

    for (let i = 0; i < testLoopCount / 4; ++i)
        shouldThrow(() => test(i), TypeError, "function is not a constructor (evaluating 'Reflect.construct(Symbol, [x])')");
})();

(function exceptions() {
    class Thrower {
        constructor(value, doThrow)
        {
            this.value = value;
            if (doThrow)
                throw new Error("thrown " + value);
        }
    }

    class OverflowThenThrow {
        constructor(a, b)
        {
            this.value = a + b;
            if (this.value > 0x7fffffff)
                throw new Error("overflow");
        }
    }

    function literal(value, doThrow)
    {
        let array = [value, doThrow];
        let copy = value;
        try {
            return Reflect.construct(Thrower, [value, doThrow]).value;
        } catch (error) {
            if (array[0] !== copy || array[1] !== doThrow)
                return "bad locals";
            return error.message;
        }
    }
    noInline(literal);

    function literalOverflow(a, b)
    {
        let array = [a, b];
        try {
            return Reflect.construct(OverflowThenThrow, [a, b]).value;
        } catch (error) {
            return error.message + array[0] + array[1];
        }
    }
    noInline(literalOverflow);

    function generic(args)
    {
        try {
            return Reflect.construct(Thrower, args).value;
        } catch (error) {
            return error.message;
        }
    }
    noInline(generic);

    let throwingArrayLike = { length: 2, 0: 1, get 1() { throw new Error("getter"); } };

    for (let i = 0; i < testLoopCount; ++i) {
        let doThrow = !(i % Math.max(2, rareStride));
        shouldBe(literal(i, doThrow), doThrow ? "thrown " + i : i);
        shouldBe(generic([i, doThrow]), doThrow ? "thrown " + i : i);
        if (doThrow)
            shouldBe(generic(throwingArrayLike), "getter");
        shouldBe(literalOverflow(i, 1), i + 1);
    }
    shouldBe(literalOverflow(0x7fffffff, 1), "overflow21474836471");
})();

(function constructorArgumentTypeChange() {
    class Adder {
        constructor(a, b)
        {
            this.sum = a + b;
        }
    }

    function test(a, b)
    {
        let object = Reflect.construct(Adder, [a, b]);
        return object.sum;
    }
    noInline(test);

    let lastSum;
    class Recorder {
        constructor(a, b)
        {
            lastSum = a + b;
        }
    }

    function testIgnoringResult(a, b)
    {
        Reflect.construct(Recorder, [a, b]);
        return lastSum;
    }
    noInline(testIgnoringResult);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(test(i, 1), i + 1);
        shouldBe(testIgnoringResult(i, 1), i + 1);
    }
    shouldBe(test(0x7fffffff, 1), 0x80000000);
    shouldBe(testIgnoringResult(0x7fffffff, 1), 0x80000000);
    shouldBe(test("a", "b"), "ab");
    shouldBe(testIgnoringResult("a", "b"), "ab");
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(test(i, 1), i + 1);
        shouldBe(testIgnoringResult(i, 1), i + 1);
    }
})();

(function polymorphicTargets() {
    class A { constructor(x) { this.x = x; this.kind = "A"; } }
    class B { constructor(x) { this.x = x; this.kind = "B"; } }
    function C(x) { this.x = x; this.kind = "C"; }

    function literal(target, x)
    {
        return Reflect.construct(target, [x]);
    }
    noInline(literal);

    let targets = [A, B, C];
    for (let i = 0; i < testLoopCount; ++i) {
        let target = targets[i % 3];
        let object = literal(target, i);
        shouldBe(object.x, i);
        shouldBe(object.kind, target.name);
    }
})();

(function callerAndStack() {
    let shouldCapture = true;

    function Sloppy()
    {
        if (!shouldCapture)
            return;
        this.caller = Sloppy.caller;
        this.stack = new Error().stack.split("\n").slice(0, 2).join("\n");
    }

    function literal()
    {
        return Reflect.construct(Sloppy, []);
    }
    noInline(literal);

    function generic(args)
    {
        return Reflect.construct(Sloppy, args);
    }
    noInline(generic);

    let literalStack = literal().stack;
    let genericStack = generic([]).stack;
    shouldBe(literalStack.split("\n")[1].startsWith("literal@"), true);
    shouldBe(genericStack.split("\n")[1].startsWith("generic@"), true);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldCapture = !(i % rareStride);
        let literalObject = literal();
        let genericObject = generic([]);
        if (!shouldCapture)
            continue;
        shouldBe(literalObject.caller, literal);
        shouldBe(literalObject.stack, literalStack);
        shouldBe(genericObject.caller, generic);
        shouldBe(genericObject.stack, genericStack);
    }
})();

(function genericArguments() {
    function construct(target, args, newTarget)
    {
        return Reflect.construct(target, args, newTarget);
    }
    noInline(construct);

    let log = [];
    let arrayLike = {
        get length() { log.push("length"); return 2; },
        get 0() { log.push("0"); return "zero"; },
        get 1() { log.push("1"); return "one"; },
    };
    let proxy = new Proxy(["a", "b"], {
        get(target, key, receiver)
        {
            log.push(String(key));
            return Reflect.get(target, key, receiver);
        }
    });
    let longArray = new Array(1000).fill(1);
    let holes = [0, 1, 2, 3, 4, 5, 6, , 8];
    Object.defineProperty(Array.prototype, 7, { get() { log.push("hole"); return "fromPrototype"; }, configurable: true });
    function Eighth() { this.eighth = arguments[7]; this.count = arguments.length; }
    let typedArray = new Int32Array([1, 2]);

    for (let i = 0; i < testLoopCount; ++i) {
        let point = construct(Point, [i, 2], Point);
        shouldBe(point.x, i);
        shouldBe(point.count, 2);
        if (i % rareStride)
            continue;

        log = [];
        point = construct(Point, arrayLike, Other);
        shouldBe(point.x, "zero");
        shouldBe(point.y, "one");
        shouldBe(Object.getPrototypeOf(point), Other.prototype);
        shouldBe(log.join(), "length,0,1");

        log = [];
        point = construct(Point, proxy, Point);
        shouldBe(point.x, "a");
        shouldBe(point.y, "b");
        shouldBe(log.join(), "length,0,1");

        log = [];
        let eighth = construct(Eighth, holes, Eighth);
        shouldBe(eighth.eighth, "fromPrototype");
        shouldBe(eighth.count, 9);
        shouldBe(log.join(), "hole");

        point = construct(Point, typedArray, Point);
        shouldBe(point.x, 1);
        shouldBe(point.y, 2);

        point = construct(Point, longArray, Point);
        shouldBe(point.count, 1000);

        point = construct(Point, function (a, b, c) { }, Point);
        shouldBe(point.count, 3);
        shouldBe(point.x, undefined);
    }
    delete Array.prototype[7];
})();
