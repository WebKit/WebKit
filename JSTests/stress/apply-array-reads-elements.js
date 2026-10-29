function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function describe() {
    let result = arguments.length + ":";
    for (let i = 0; i < arguments.length; ++i)
        result += (Object.is(arguments[i], -0) ? "-0" : String(arguments[i])) + ",";
    return result;
}
noInline(describe);

function describeInlined(a, b, c) {
    return arguments.length + ":" + String(a) + "," + String(b) + "," + String(c);
}

function apply(array) { return describe.apply(undefined, array); }
noInline(apply);
function applyInlined(array) { return describeInlined.apply(undefined, array); }
noInline(applyInlined);
function applyHoley(array) { return describe.apply(undefined, array); }
noInline(applyHoley);
function applyHoleyInlined(array) { return describeInlined.apply(undefined, array); }
noInline(applyHoleyInlined);
function applyAny(array) { return describe.apply(undefined, array); }
noInline(applyAny);

class Derived extends Array { }

const int32s = [1, 2, 3];
const doubles = [1.5, -0, 2.5];
const objects = ["a", {}, null];
const empty = [];
const withHole = [1, , 3];
const doublesWithHole = [1.5, , 2.5];
const undecided = new Array(2);
const frozen = Object.freeze([1, 2, 3]);
const sparse = [];
sparse[100] = 1;
const derived = Derived.from([1, 2, 3]);
const withGetter = [1, 2, 3];
Object.defineProperty(withGetter, 1, { get() { return "getter"; } });
const arrayLike = { length: 2, 0: "x", 1: "y" };

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(apply(int32s), "3:1,2,3,");
    shouldBe(apply(doubles), "3:1.5,-0,2.5,");
    shouldBe(apply(objects), "3:a,[object Object],null,");
    shouldBe(apply(empty), "0:");
    shouldBe(applyInlined(int32s), "3:1,2,3");
    shouldBe(applyInlined(doubles), "3:1.5,0,2.5");
    shouldBe(applyInlined(empty), "0:undefined,undefined,undefined");

    shouldBe(applyHoley(withHole), "3:1,undefined,3,");
    shouldBe(applyHoley(doublesWithHole), "3:1.5,undefined,2.5,");
    shouldBe(applyHoley(undecided), "2:undefined,undefined,");
    shouldBe(applyHoleyInlined(withHole), "3:1,undefined,3");
    shouldBe(applyHoleyInlined(doublesWithHole), "3:1.5,undefined,2.5");
    shouldBe(applyHoleyInlined(undecided), "2:undefined,undefined,undefined");

    shouldBe(applyAny(frozen), "3:1,2,3,");
    shouldBe(applyAny(sparse).length, 4 + 100 * 10 + 2);
    shouldBe(applyAny(derived), "3:1,2,3,");
    shouldBe(applyAny(withGetter), "3:1,getter,3,");
    shouldBe(applyAny(arrayLike), "2:x,y,");
    shouldBe(applyAny(null), "0:");
    shouldBe(applyAny(undefined), "0:");
}

Array.prototype[1] = "fromPrototype";
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(apply(int32s), "3:1,2,3,");
    shouldBe(applyHoley(withHole), "3:1,fromPrototype,3,");
    shouldBe(applyHoley(doublesWithHole), "3:1.5,fromPrototype,2.5,");
    shouldBe(applyHoley(undecided), "2:undefined,fromPrototype,");
    shouldBe(applyHoleyInlined(withHole), "3:1,fromPrototype,3");
    shouldBe(applyHoleyInlined(undecided), "2:undefined,fromPrototype,undefined");
}
