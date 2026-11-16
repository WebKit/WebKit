function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function firstTwo(string) { let [a, b] = string; return a + "|" + b; }
function firstAndRest(string) { let [a, ...rest] = string; return a + "|" + rest.join("|"); }
function third(string) { let [, , c = "default"] = string; return c; }
function none(string) { let [] = string; return string.length; }
function swapped(string) { let a, b; [a, b] = string; [a, b] = [b, a]; return a + "|" + b; }
function parameter([a, b], [c] = "z") { return a + "|" + b + "|" + c; }
noInline(firstTwo); noInline(firstAndRest); noInline(third); noInline(none); noInline(swapped); noInline(parameter);

let cases = [
    ["", []],
    ["a", ["a"]],
    ["abcd", ["a", "b", "c", "d"]],
    ["a\u{1F600}b\u{1F601}", ["a", "\u{1F600}", "b", "\u{1F601}"]],
    ["\u{1F600}\u{1F601}", ["\u{1F600}", "\u{1F601}"]],
    ["a\ud83db", ["a", "\ud83d", "b"]],
    ["\ude00\ud83d", ["\ude00", "\ud83d"]],
];

for (let i = 0; i < testLoopCount; i++) {
    let [string, expected] = cases[i % cases.length];
    shouldBe(firstTwo(string), expected[0] + "|" + expected[1]);
    shouldBe(firstAndRest(string), expected[0] + "|" + expected.slice(1).join("|"));
    shouldBe(third(string), expected.length > 2 ? expected[2] : "default");
    shouldBe(none(string), string.length);
    shouldBe(swapped(string), expected[1] + "|" + expected[0]);
    shouldBe(parameter(string), expected[0] + "|" + expected[1] + "|z");
    shouldBe(parameter(string, string), expected[0] + "|" + expected[1] + "|" + expected[0]);
}
