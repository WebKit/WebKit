function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function collect(string) { let seen = []; for (let x of string) seen.push(x); return seen.join("|"); }
function breakAt(string, count) { let seen = []; for (let x of string) { seen.push(x); if (seen.length === count) break; } return seen.join("|"); }
function nested(string) { let seen = []; for (let x of string) { for (let y of string) { seen.push(x + y); if (y === "b") break; } } return seen.join("|"); }
function reassigned(string) { let seen = []; for (let x of string) { seen.push(x); string = "zzz"; } return seen.join("|"); }
noInline(collect); noInline(breakAt); noInline(nested); noInline(reassigned);

function rope(string) { return string.length < 2 ? string : string.substring(0, 1) + string.substring(1); }

let cases = [
    ["", []],
    ["a", ["a"]],
    ["abcd", ["a", "b", "c", "d"]],
    ["éèx", ["é", "è", "x"]],
    ["あい", ["あ", "い"]],
    ["a\u{1F600}b\u{1F601}", ["a", "\u{1F600}", "b", "\u{1F601}"]],
    ["a\ud83db", ["a", "\ud83d", "b"]],
    ["a\ude00b", ["a", "\ude00", "b"]],
    ["ab\ud83d", ["a", "b", "\ud83d"]],
    ["\ude00\ud83d", ["\ude00", "\ud83d"]],
];

for (let i = 0; i < testLoopCount; i++) {
    let [string, expected] = cases[i % cases.length];
    shouldBe(collect(string), expected.join("|"));
    shouldBe(collect(rope(string)), expected.join("|"));
    shouldBe(collect("x" + string + "y"), ["x", ...expected, "y"].join("|"));
    shouldBe(collect(("xy" + string).substring(2)), expected.join("|"));
    shouldBe(breakAt(string, 2), expected.slice(0, 2).join("|"));
    shouldBe(nested("abc"), "aa|ab|ba|bb|ca|cb");
    shouldBe(reassigned("abc"), "a|b|c");
}
