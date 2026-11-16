function shouldBe(actual, expected) {
    if (typeof actual === "object")
        actual = actual[0] + ":" + Array.prototype.slice.call(actual[1]).map(String).join(",");
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function callee(a, b, c, d) {
    return [arguments.length, [a, b, c, d]];
}

function all() {
    return [arguments.length, arguments];
}

function last() {
    return arguments[arguments.length - 1];
}

const receiver = {
    name: "receiver",
    method(a, b, c) { return [this.name + ":" + arguments.length, [a, b, c]]; },
};

function prefix(x, iterable) { return callee(x, ...iterable); }
noInline(prefix);
function suffix(iterable, x) { return callee(...iterable, x); }
noInline(suffix);
function both(x, iterable, y) { return callee(x, ...iterable, y); }
noInline(both);
function two(iterable1, iterable2) { return callee(...iterable1, ...iterable2); }
noInline(two);
function interleaved(iterable1, x, iterable2, y, iterable3) { return all(...iterable1, x, 1, ...iterable2, y, 2.5, ...iterable3); }
noInline(interleaved);
function same(x, iterable) { return all(x, ...iterable, x, ...iterable, x, ...iterable, x); }
noInline(same);
function lastOfPrefix(x, iterable) { return last(x, ...iterable); }
noInline(lastOfPrefix);
function lastOfSuffix(iterable, x) { return last(...iterable, x); }
noInline(lastOfSuffix);
function method(x, iterable) { return receiver.method(x, ...iterable); }
noInline(method);
function constants(x) { return callee(x, ...[1.5, 2.5], "end"); }
noInline(constants);
function constantPlainArguments(iterable) { return callee(null, ...iterable, undefined, 0.5); }
noInline(constantPlainArguments);
function unboxedPlainArguments(x, y, iterable) { return callee(x | 0, ...iterable, y * 0.5, x === y); }
noInline(unboxedPlainArguments);

// The constant array and the rest are eliminated, while the spread of the global is not.
var globalArray = [10, 20];
function constantDoublesAndMaterialized() { return all(...[1.5, 2.5], ...globalArray, ...["x", "y"]); }
noInline(constantDoublesAndMaterialized);
function restAndMaterialized(x, ...rest) { return all(x, ...rest, ...globalArray, ...rest, x); }
noInline(restAndMaterialized);
function inlinedRestAndMaterialized(x, ...rest) { return all(...globalArray, ...rest, x); }
function callInlinedRest0() { return inlinedRestAndMaterialized(); }
noInline(callInlinedRest0);
function callInlinedRest3() { return inlinedRestAndMaterialized(1, 2, 3); }
noInline(callInlinedRest3);
function callInlinedRestVarargs(x, array) { return inlinedRestAndMaterialized(x, ...array); }
noInline(callInlinedRestVarargs);

function* yieldBetweenSpreads(x, iterable1, iterable2) { return all(x, ...iterable1, yield, ...iterable2); }
noInline(yieldBetweenSpreads);
function callYieldBetweenSpreads(x, iterable1, y, iterable2) {
    const generator = yieldBetweenSpreads(x, iterable1, iterable2);
    generator.next();
    return generator.next(y).value;
}
noInline(callYieldBetweenSpreads);

function* generate() {
    yield 1;
    yield 2;
}
function createArguments() { return arguments; }

const int32s = [1, 2, 3];
const doubles = [1.5, -0, 2.5];
const objects = ["a", {}, null];
const empty = [];
const five = [1, 2, 3, 4, 5];
const withHole = [1, , 3];
const doublesWithHole = [1.5, , 2.5];
const set = new Set(["a", "b", "c"]);
const emptySet = new Set;

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(prefix(0, int32s), "4:0,1,2,3");
    shouldBe(prefix(0.5, doubles), "4:0.5,1.5,0,2.5");
    shouldBe(prefix("x", objects), "4:x,a,[object Object],null");
    shouldBe(prefix("x", empty), "1:x,undefined,undefined,undefined");
    shouldBe(prefix("x", five), "6:x,1,2,3");
    shouldBe(prefix("x", withHole), "4:x,1,undefined,3");
    shouldBe(prefix("x", doublesWithHole), "4:x,1.5,undefined,2.5");
    shouldBe(prefix("x", set), "4:x,a,b,c");
    shouldBe(prefix("x", emptySet), "1:x,undefined,undefined,undefined");
    shouldBe(prefix("x", "yz"), "3:x,y,z,undefined");
    shouldBe(prefix("x", generate()), "3:x,1,2,undefined");
    shouldBe(prefix("x", createArguments(1, 2)), "3:x,1,2,undefined");

    shouldBe(suffix(int32s, "x"), "4:1,2,3,x");
    shouldBe(suffix(empty, "x"), "1:x,undefined,undefined,undefined");
    shouldBe(suffix(doubles, 0.5), "4:1.5,0,2.5,0.5");
    shouldBe(suffix(five, "x"), "6:1,2,3,4");

    shouldBe(both("x", int32s, "y"), "5:x,1,2,3");
    shouldBe(both("x", empty, "y"), "2:x,y,undefined,undefined");
    shouldBe(both("x", [1.5], "y"), "3:x,1.5,y,undefined");

    shouldBe(two(empty, empty), "0:undefined,undefined,undefined,undefined");
    shouldBe(two(int32s, empty), "3:1,2,3,undefined");
    shouldBe(two(empty, doubles), "3:1.5,0,2.5,undefined");
    shouldBe(two(int32s, doubles), "6:1,2,3,1.5");

    shouldBe(interleaved(empty, "x", empty, "y", empty), "4:x,1,y,2.5");
    shouldBe(interleaved(int32s, "x", empty, "y", doubles), "10:1,2,3,x,1,y,2.5,1.5,0,2.5");
    shouldBe(interleaved(empty, "x", five, "y", empty), "9:x,1,1,2,3,4,5,y,2.5");
    shouldBe(interleaved(doubles, "x", set, "y", "z"), "11:1.5,0,2.5,x,1,a,b,c,y,2.5,z");

    shouldBe(same("x", empty), "4:x,x,x,x");
    shouldBe(same("x", [1, 2.5]), "10:x,1,2.5,x,1,2.5,x,1,2.5,x");

    shouldBe(Object.is(lastOfPrefix(1, [1.5, -0]), -0), true);
    shouldBe(lastOfPrefix("x", empty), "x");
    shouldBe(lastOfPrefix("x", five), 5);
    shouldBe(lastOfSuffix(empty, "x"), "x");
    shouldBe(lastOfSuffix(five, "x"), "x");

    shouldBe(method("x", int32s), "receiver:4:x,1,2");
    shouldBe(method("x", empty), "receiver:1:x,undefined,undefined");

    shouldBe(constants("x"), "4:x,1.5,2.5,end");
    shouldBe(constantPlainArguments(empty), "3:null,undefined,0.5,undefined");
    shouldBe(constantPlainArguments([1.5]), "4:null,1.5,undefined,0.5");
    shouldBe(unboxedPlainArguments(1, 3, empty), "3:1,1.5,false,undefined");
    shouldBe(unboxedPlainArguments(3, 3, [0.5]), "4:3,0.5,1.5,true");

    shouldBe(constantDoublesAndMaterialized(), "6:1.5,2.5,10,20,x,y");
    shouldBe(restAndMaterialized("x"), "4:x,10,20,x");
    shouldBe(restAndMaterialized("x", 1, 2.5), "8:x,1,2.5,10,20,1,2.5,x");
    shouldBe(callInlinedRest0(), "3:10,20,undefined");
    shouldBe(callInlinedRest3(), "5:10,20,2,3,1");
    shouldBe(callInlinedRestVarargs("x", empty), "3:10,20,x");
    shouldBe(callInlinedRestVarargs("x", doubles), "6:10,20,1.5,0,2.5,x");

    shouldBe(callYieldBetweenSpreads("x", [1.5, 2.5], "y", int32s), "7:x,1.5,2.5,y,1,2,3");
    shouldBe(callYieldBetweenSpreads("x", empty, "y", empty), "2:x,y");
}

Array.prototype[1] = "fromPrototype";
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(prefix("x", int32s), "4:x,1,2,3");
    shouldBe(prefix("x", withHole), "4:x,1,fromPrototype,3");
    shouldBe(both("x", doublesWithHole, "y"), "5:x,1.5,fromPrototype,2.5");
}
