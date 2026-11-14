//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((typeof message === "function" ? message() : message) + ": got " + actual + ", expected " + expected);
}

function indexOf(string, search) { return string.indexOf(search); }
noInline(indexOf);
function indexOfFrom(string, search, position) { return string.indexOf(search, position | 0); }
noInline(indexOfFrom);

function expectedIndexOf(string, search, position) {
    if (position < 0)
        position = 0;
    if (position > string.length)
        position = string.length;
    for (let i = position; i + search.length <= string.length; ++i) {
        let j = 0;
        while (j < search.length && string.charCodeAt(i + j) === search.charCodeAt(j))
            ++j;
        if (j === search.length)
            return i;
    }
    return -1;
}

function flat(string)
{
    string.charCodeAt(0);
    return string;
}
noInline(flat);
noDFG(flat);

function ropeSubstring(string, start, end) { return string.substring(start, end); }
noInline(ropeSubstring);
noDFG(ropeSubstring);

function wide(string)
{
    let result = $vm.make16BitStringIfPossible(string);
    shouldBe(is8BitString(result), false, "wide");
    return result;
}

let subjects = [
    "",
    "a",
    "\u3042",
    "abcabc",
    wide("abcabc"),
    "caf\u00e9 \u00ff\u0000",
    "\u3042a\u3044\u00e9b\u0000\u3042",
    "\ud83d\ude00x\ud83d\ude00",
    flat("x".repeat(15) + "a" + "x".repeat(16) + "b"),
    flat("\u3044".repeat(7) + "a" + "\u3044".repeat(9) + "\u3042"),
];

let searches = [
    "", "a", "b", "z", "\u0000", "\u00e9", "\u00ff",
    "\u0161", "\u3061", "\u3042",
    "\ud83d", "\ude00",
    "bc", "ab", "\u3042a",
];

let positions = [-2147483648, -1, 0, 1, 2, 5, 6, 7, 15, 16, 31, 32, 33, 100, 2147483647];

let cases = [];
for (let subject of subjects) {
    for (let search of searches) {
        cases.push([subject, search, 0, expectedIndexOf(subject, search, 0)]);
        for (let position of positions)
            cases.push([subject, search, position, expectedIndexOf(subject, search, position)]);
    }
}

for (let round = 0; round < 20; ++round) {
    for (let [subject, search, position, expected] of cases) {
        shouldBe(indexOfFrom(subject, search, position), expected, () => "indexOfFrom " + escape(subject) + " " + escape(search) + " " + position);
        if (!position)
            shouldBe(indexOf(subject, search), expected, () => "indexOf " + escape(subject) + " " + escape(search));
    }
}

let left = flat("x".repeat(200) + "a");
let right = flat("y".repeat(200) + "b");
let wideRight = flat("\u3044".repeat(200) + "\u3042");
for (let string of [...subjects, ...searches, left, right, wideRight])
    shouldBe(isRope(string), false, "flat");

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(indexOf(left + right, "b"), 401, "rope");
    shouldBe(indexOfFrom(left + right, "a", 201), -1, "rope from");
    shouldBe(indexOf(left + wideRight, "\u3042"), 401, "wide rope");
    shouldBe(indexOf("x" + i, "x"), 0, "short rope");
    shouldBe(indexOf(left.substring(1, 201), "a"), 199, "substring");
    shouldBe(indexOfFrom(left.substring(1, 201), "a", 200), -1, "substring from");
}

let ropeSearch = ropeSubstring(wideRight, 200, 201);
let substring = ropeSubstring(wideRight, 1, 201);
shouldBe(ropeSearch.length, 1, "rope search length");
shouldBe(isRope(ropeSearch), true, "rope search before");
shouldBe(isRope(substring), true, "substring before");

let wideFlat = flat("a\u3044\u3042b\u3042");
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(indexOf(wideFlat, ropeSearch), 2, "flat, rope search");
    shouldBe(indexOfFrom(wideFlat, ropeSearch, 3), 4, "flat, rope search from");
    shouldBe(indexOfFrom(wideFlat, ropeSearch, 5), -1, "flat, rope search past the end");
    shouldBe(indexOf("abc", ropeSearch), -1, "8-bit flat, rope search");
    shouldBe(indexOf(left + wideRight, ropeSearch), 401, "rope, rope search");
    shouldBe(indexOf(substring, ropeSearch), 199, "substring, rope search");
    shouldBe(indexOf(ropeSearch, ropeSearch), 0, "rope search in itself");
    shouldBe(indexOf(ropeSearch, "\u3042"), 0, "one-character rope, flat search");
}

shouldBe(isRope(ropeSearch), true, "rope search after");
shouldBe(isRope(substring), true, "substring after");
