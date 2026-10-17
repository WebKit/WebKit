function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function describe() {
    let result = arguments.length + ":";
    for (let i = 0; i < arguments.length; ++i)
        result += String(arguments[i]) + ",";
    return result;
}
noInline(describe);

function describeInlined(a, b, c) {
    return arguments.length + ":" + String(a) + "," + String(b) + "," + String(c);
}

function call(iterable) { return describe(...iterable); }
noInline(call);
function callInlined(iterable) { return describeInlined(...iterable); }
noInline(callInlined);
function callWithThis(iterable) { return describe.call(...iterable); }
noInline(callWithThis);
function callSet(set) { return describe(...set); }
noInline(callSet);
function callString(string) { return describe(...string); }
noInline(callString);
class Point {
    constructor(x, y, z) {
        this.result = arguments.length + ":" + String(x) + "," + String(y) + "," + String(z);
    }
}
function construct(iterable) { return new Point(...iterable); }
noInline(construct);

function* generate() {
    yield 1;
    yield 2;
}
function createArguments() { return arguments; }

const set = new Set(["a", "b", "c"]);
const emptySet = new Set;
const map = new Map([[1, 2]]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(callSet(set), "3:a,b,c,");
    shouldBe(callSet(emptySet), "0:");
    shouldBe(callString("xyz"), "3:x,y,z,");
    shouldBe(callString(""), "0:");

    shouldBe(call(set), "3:a,b,c,");
    shouldBe(call("xy"), "2:x,y,");
    shouldBe(call(generate()), "2:1,2,");
    shouldBe(call(map.keys()), "1:1,");
    shouldBe(call(createArguments(1, 2, 3)), "3:1,2,3,");
    shouldBe(call([1, 2]), "2:1,2,");

    shouldBe(callInlined(set), "3:a,b,c");
    shouldBe(callInlined("xy"), "2:x,y,undefined");
    shouldBe(callInlined(generate()), "2:1,2,undefined");
    shouldBe(callInlined([1, 2]), "2:1,2,undefined");

    shouldBe(construct(set).result, "3:a,b,c");
    shouldBe(construct("xy").result, "2:x,y,undefined");
    shouldBe(construct(emptySet).result, "0:undefined,undefined,undefined");

    shouldBe(callWithThis(set), "2:b,c,");
    shouldBe(callWithThis(emptySet), "0:");
    shouldBe(callWithThis("x"), "0:");
}
