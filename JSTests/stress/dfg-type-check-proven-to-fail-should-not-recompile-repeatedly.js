//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--useConcurrentJIT=false")

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`expected ${expected} but got ${actual}`);
}

const string = "abcdefghijklmnopqrstuvwxyz";
const doubles = new Float64Array([0, 1, 2, 3, 4, 5, 6, 7]);
const typedArray = new Uint8Array(16);
let functionCount = 0;

function run(body, count, expected) {
    const func = new Function("string", "array", "doubles", "typedArray", "i", `${body} // ${functionCount++}`);
    noInline(func);
    for (let i = 0; i < count; i++)
        shouldBe(func(string, [1, 2, 3, 4, 5, 6], doubles, typedArray, i), expected(i));
    shouldBe(numberOfDFGCompiles(func) <= 3, true);
}

function testDFG(body, expected) {
    run(body, testLoopCount * 2, expected);
}

function testFTL(body, expected) {
    run(`for (let j = 0; j < 3000; j++); ${body}`, testLoopCount / 2, expected);
}

testDFG("return array.splice(i & 3, Infinity).length;", i => 6 - (i & 3));
testDFG("return array.splice(i & 3, i >= 0).length;", i => 1);
testDFG("return array.slice(i & 3, Infinity).length;", i => 6 - (i & 3));
testDFG("return array.indexOf(i & 3, undefined);", i => (i & 3) - 1);
testDFG("return string.slice(doubles[i & 3], doubles[(i & 3) + 4]).length;", i => 4);
testDFG("return string.charAt(undefined);", i => "a");
testDFG("return i.toString(10.5);", i => String(i));
testDFG("let value = undefined; value++; return value;", i => NaN);
testDFG("typedArray.set(i & 7, 0); return typedArray[0];", i => 0);
testDFG("return new DataView(typedArray.buffer).getInt16(0, i & 1);", i => 0);
testDFG("return [Math.imul][0]({}, i);", i => 0);

testFTL("return array.splice(i & 3, Infinity).length;", i => 6 - (i & 3));
testFTL("return array.splice(i & 3, i >= 0).length;", i => 1);
testFTL("let value = undefined; value++; return value;", i => NaN);
testFTL("typedArray.set(i & 7, 0); return typedArray[0];", i => 0);
testFTL("return new DataView(typedArray.buffer).getInt16(0, i & 1);", i => 0);
