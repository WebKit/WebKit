function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function flatten(string) {
    return string.split("").join("");
}
noInline(flatten);

let latin1 = "the quick brown fox jumps over the lazy dog, caf\u00e9 na\u00efve, and then rests for a while under a tree";
let escapes = "the \"quick\" brown \\fox\\ jumps\nover\tthe lazy dog and then \u0001 rests for a while under a tree";
let wide = "the quick \u65e5\u672c\u8a9e fox jumps over \u4e16\u754c the \ud83d\ude80 dog and then rests for a while under a tree";
let wideEscapes = "the \"quick\" \u65e5\u672c\u8a9e \\fox\\ jumps\nover \ud83d\ude80 the lazy dog and then rests for a while";
let bases = [latin1, escapes, wide, wideEscapes];
let ranges = [[0, 40], [3, 6], [10, 17], [5, 20], [9, 41], [21, 22], [1, 70], [30, 31], [29, 31], [0, 1]];

function check(base, start, end) {
    let expected = flatten(base.slice(start, end));

    shouldBe(JSON.stringify(base.slice(start, end)), JSON.stringify(expected));
    shouldBe(JSON.stringify({ id: 1, text: base.slice(start, end), tail: base.slice(start) }), JSON.stringify({ id: 1, text: expected, tail: flatten(base.slice(start)) }));
    shouldBe(JSON.stringify([base.slice(start, end), 1, base.slice(start, end)]), JSON.stringify([expected, 1, expected]));
    shouldBe(JSON.stringify({ text: base.slice(start, end), list: [base.slice(start, end)] }, null, 2), JSON.stringify({ text: expected, list: [expected] }, null, 2));
    shouldBe(JSON.stringify({ text: base.slice(start, end) }, ["text"]), JSON.stringify({ text: expected }, ["text"]));
    shouldBe(JSON.stringify(base.slice(1).slice(start, end)), JSON.stringify(flatten(base.slice(start + 1, end + 1))));

    let substring = base.slice(start, end);
    let object = { text: substring };
    shouldBe(JSON.stringify(object), JSON.stringify({ text: expected }));
    shouldBe(JSON.stringify(object), JSON.stringify({ text: expected }));
    shouldBe(substring, expected);
    shouldBe(JSON.parse(JSON.stringify(object)).text, expected);
}
noInline(check);

for (let base of bases) {
    for (let [start, end] of ranges)
        check(base, start, end);
}

let rocket = wide.indexOf("\ud83d\ude80");
shouldBe(JSON.stringify(wide.slice(0, rocket + 1)), "\"the quick \u65e5\u672c\u8a9e fox jumps over \u4e16\u754c the \\ud83d\"");
shouldBe(JSON.stringify({ text: wide.slice(rocket + 1, rocket + 9) }), "{\"text\":\"\\ude80 dog an\"}");
shouldBe(JSON.stringify(escapes.slice(2, 19)), "\"e \\\"quick\\\" brown \\\\\"");
shouldBe(JSON.stringify({ text: latin1.slice(45, 55) }), "{\"text\":\"caf\u00e9 na\u00efve\"}");

function stringify(base, start, end) {
    return JSON.stringify({ id: start, text: base.slice(start, end) });
}
noInline(stringify);

let expected = bases.map((base) => `{"id":4,"text":${JSON.stringify(flatten(base.slice(4, 44)))}}`);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(stringify(bases[i % bases.length], 4, 44), expected[i % bases.length]);
