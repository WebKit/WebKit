function shouldBe(actual, expected, string, atom) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected + " for " + JSON.stringify(string) + " === " + JSON.stringify(atom));
}

const atoms = [
    "", "a", "if", "var", "this", "while", "return", "default", "function",
    "abcdefghi", "instanceof", "abcdefghijk", "abcdefghijkl", "abcdefghijklm",
    "abcdefghijklmn", "abcdefghijklmno", "abcdefghijklmnop", "abcdefghijklmnopq",
];

function test0(s) { return s === ""; }
function test1(s) { return s === "a"; }
function test2(s) { return s === "if"; }
function test3(s) { return s === "var"; }
function test4(s) { return s === "this"; }
function test5(s) { return s === "while"; }
function test6(s) { return s === "return"; }
function test7(s) { return s === "default"; }
function test8(s) { return s === "function"; }
function test9(s) { return "abcdefghi" === s; }
function test10(s) { return s === "instanceof"; }
function test11(s) { return s === "abcdefghijk"; }
function test12(s) { return s === "abcdefghijkl"; }
function test13(s) { return s === "abcdefghijklm"; }
function test14(s) { return s === "abcdefghijklmn"; }
function test15(s) { return s === "abcdefghijklmno"; }
function test16(s) { return s === "abcdefghijklmnop"; }
function test17(s) { return s === "abcdefghijklmnopq"; }
const tests = [test0, test1, test2, test3, test4, test5, test6, test7, test8, test9, test10, test11, test12, test13, test14, test15, test16, test17];
for (const test of tests)
    noInline(test);

function keyword(id) {
    switch (id.length) {
    case 5:
        return id === "while" || id === "break" || id === "catch" || id === "throw" || id === "const";
    default:
        return false;
    }
}
noInline(keyword);

function flatten(string) {
    string.indexOf("#");
    return string;
}

const source8 = flatten("  " + atoms.join(" ") + " ");
const source16 = flatten("\u3042" + source8);

function candidates(atom) {
    const result = [];
    for (const source of [source8, source16]) {
        const index = source.indexOf(" " + atom + " ") + 1;
        result.push([source.slice(index, index + atom.length), true]);
    }
    result.push([atom.split("").join(""), true]);
    if (atom.length) {
        const half = atom.length >> 1;
        result.push([atom.slice(0, half) + atom.slice(half), true]);
        result.push([flatten("\u3042" + atom + " ").slice(1, 1 + atom.length), true]);
        for (let i = 0; i < atom.length; ++i) {
            const mutated = atom.slice(0, i) + String.fromCharCode(atom.charCodeAt(i) ^ 1) + atom.slice(i + 1);
            result.push([mutated, false]);
            const wide = flatten("\u3042" + mutated + "  ");
            result.push([wide.slice(1, 1 + atom.length), false]);
            const narrow = flatten("_" + mutated + "  ");
            result.push([narrow.slice(1, 1 + atom.length), false]);
        }
        result.push([atom.slice(1), false]);
        result.push([source8.slice(2, 2 + atom.length + 1), false]);
    }
    result.push([atom + "x", false]);
    result.push([flatten("y" + atom + "x").slice(1), false]);
    return result;
}

function equalsByCharacter(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; ++i) {
        if (a.charCodeAt(i) !== b.charCodeAt(i))
            return false;
    }
    return true;
}
noDFG(equalsByCharacter);

const cases = atoms.map(candidates);
const keywords = ["while", "break", "catch", "throw", "const"];
const keywordCandidates = [...keywords, "whilf", "count", "const ", "wh", "xyzzy"];
const keywordSource = flatten("." + keywordCandidates.join(".") + ".");
const keywordSlices = keywordCandidates.map(word => {
    const index = keywordSource.indexOf("." + word + ".") + 1;
    return keywordSource.slice(index, index + word.length);
});

function untyped(value) { return value === "while"; }
noInline(untyped);
function notDouble(value) { return "abcdefghij" === value; }
noInline(notDouble);

const whileSource = flatten(".while.whilf.");
const untypedValues = [
    [whileSource.slice(1, 6), true], [whileSource.slice(7, 12), false], ["wh" + "ile".split("").join(""), true],
    [flatten("\u3042while").slice(1), true], ["while", true], [42, false], [4.2, false], [undefined, false],
    [null, false], [{}, false], [Symbol(), false], [true, false], [10n, false],
];
const abcSource = flatten(".abcdefghij.abcdefghik.");
const notDoubleValues = [
    [abcSource.slice(1, 11), true], [abcSource.slice(12, 22), false], ["abcde" + "fghij".split("").join(""), true],
    [flatten("\u3042abcdefghij").slice(1), true], ["abcdefghij", true], [42, false], [undefined, false],
    [null, false], [{}, false], [Symbol(), false], [false, false],
];

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (let i = 0; i < tests.length; ++i) {
        const [string, expected] = cases[i][iteration % cases[i].length];
        shouldBe(tests[i](string), expected, string, atoms[i]);
        const other = cases[(i + 1) % tests.length];
        const otherString = other[iteration % other.length][0];
        shouldBe(tests[i](otherString), equalsByCharacter(otherString, atoms[i]), otherString, atoms[i]);
    }
    const index = iteration % keywordCandidates.length;
    shouldBe(keyword(keywordSlices[index]), keywords.includes(keywordCandidates[index]), keywordSlices[index], "keyword");

    const [untypedValue, untypedExpected] = untypedValues[iteration % untypedValues.length];
    shouldBe(untyped(untypedValue), untypedExpected, String(untypedValue), "while");
    const [notDoubleValue, notDoubleExpected] = notDoubleValues[iteration % notDoubleValues.length];
    shouldBe(notDouble(notDoubleValue), notDoubleExpected, String(notDoubleValue), "abcdefghij");
}
