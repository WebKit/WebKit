function format(value) {
    return Array.isArray(value) ? value.map(format).join(",") : String(value);
}

function shouldBe(actual, expected) {
    if (Array.isArray(actual))
        actual = format(actual);
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function none() { return arguments.length; }
// These are small enough for the DFG to inline them.
function one(a) { return [arguments.length, a]; }
function three(a, b, c) { return [arguments.length, a, b, c]; }
function rest(a, ...array) { return [arguments.length, a, array.length]; }
function defaults(a = 1, b = 2) { return [arguments.length, a, b]; }
function assigned(a, b) {
    a = a === undefined ? 10 : 20;
    b = b === undefined ? a + 1 : a + 2;
    return a + b;
}
function eight(a, b, c, d, e, f, g, h) {
    "use strict";
    return [a, b, c, d, e, f, g, h].every(value => value === undefined) && this === 42;
}

// All of these parameters must be filled, even though Options::maximumVarargsForInlining() is 100.
const parameters = [];
for (let i = 0; i < 150; ++i)
    parameters.push("p" + i);
const many = new Function(...parameters, "return p0 === undefined && p75 === undefined && p149 === undefined && arguments.length === 0;");

function callNone(args) { return none.apply(null, args); }
noInline(callNone);
function callOne(args) { return one.apply(null, args); }
noInline(callOne);
function callThree(args) { return three.apply(null, args); }
noInline(callThree);
function callRest(args) { return rest.apply(null, args); }
noInline(callRest);
function callDefaults(args) { return defaults.apply(null, args); }
noInline(callDefaults);
function callAssigned(args) { return assigned.apply(null, args); }
noInline(callAssigned);
function callEight(args) { return eight.apply(42, args); }
noInline(callEight);
function callMany(args) { return many.apply(null, args); }
noInline(callMany);
function tail(args) { "use strict"; return three.apply(null, args); }
noInline(tail);

// Values live across the call must survive, and so must the slots next to the callee's frame.
function live(x, y, z, args) {
    let object = { x };
    let sum = y * 1.5;
    let array = three.apply(object, args);
    return [array, object.x, sum, y + z];
}
noInline(live);

// The callee OSR exits after its arguments are filled, since the value is not a number in the end.
var globalValue = 0;
function exiting(a, b, c) {
    return [globalValue + 1, arguments.length, a, b, c];
}
function callExiting(args) { return exiting.apply(null, args); }
noInline(callExiting);

// The call site has seen more arguments than the callee has parameters, so some slots stay unfilled.
function inlinedWrapper(args) { return three.apply(null, args); }
function callInlinedWrapper() { return inlinedWrapper(null); }
noInline(callInlinedWrapper);
function inlinedRestWrapper(args) { return rest.apply(null, args); }
function callInlinedRestWrapper() { return inlinedRestWrapper(undefined); }
noInline(callInlinedRestWrapper);

// Leave values other than undefined where the frames of the next call go.
const garbage = new Array(400).fill("garbage");
function pollute() { return arguments.length; }
noInline(pollute);
function call(func, ...args) {
    pollute.apply(null, garbage);
    return func(...args);
}
noInline(call);

for (let i = 0; i < 100; ++i) {
    shouldBe(inlinedWrapper([1, 2, 3, 4, 5, 6]), "6,1,2,3");
    shouldBe(inlinedRestWrapper([1, 2, 3, 4, 5, 6]), "6,1,5");
}

for (let i = 0; i < testLoopCount; ++i) {
    const args = i & 1 ? null : undefined;
    shouldBe(call(callNone, args), 0);
    shouldBe(call(callOne, args), "0,undefined");
    shouldBe(call(callThree, args), "0,undefined,undefined,undefined");
    shouldBe(call(callRest, args), "0,undefined,0");
    shouldBe(call(callDefaults, args), "0,1,2");
    shouldBe(call(callAssigned, args), 21);
    shouldBe(call(callEight, args), true);
    shouldBe(call(callMany, args), true);
    shouldBe(call(tail, args), "0,undefined,undefined,undefined");
    shouldBe(call(live, "x", i, 2, args), `0,undefined,undefined,undefined,x,${i * 1.5},${i + 2}`);
    globalValue = i;
    shouldBe(call(callExiting, args), `${i + 1},0,undefined,undefined,undefined`);
    shouldBe(call(callInlinedWrapper), "0,undefined,undefined,undefined");
    shouldBe(call(callInlinedRestWrapper), "0,undefined,0");
}

globalValue = "s";
shouldBe(call(callExiting, null), "s1,0,undefined,undefined,undefined");
shouldBe(call(callExiting, undefined), "s1,0,undefined,undefined,undefined");

// The speculation that the arguments are undefined or null fails.
for (let i = 0; i < 10; ++i) {
    shouldBe(call(callNone, [1]), 1);
    shouldBe(call(callOne, [1]), "1,1");
    shouldBe(call(callThree, [1, 2]), "2,1,2,undefined");
    shouldBe(call(callRest, [1, 2, 3]), "3,1,2");
    shouldBe(call(callDefaults, [5]), "1,5,2");
    shouldBe(call(callAssigned, [1, 2]), 42);
    shouldBe(call(callEight, [1]), false);
    shouldBe(call(callMany, [1]), false);
    shouldBe(call(tail, [1, 2, 3, 4]), "4,1,2,3");
    shouldBe(call(live, "x", 1, 2, [7]), "1,7,undefined,undefined,x,1.5,3");
    shouldBe(call(callThree, null), "0,undefined,undefined,undefined");
}
