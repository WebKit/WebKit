function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function bitOr(s, i) { return s.charCodeAt(i) | 0; }
noInline(bitOr);
function bitAnd(s, i) { return s.charCodeAt(i) & 0xff; }
noInline(bitAnd);
function plusOne(s, i) { return s.charCodeAt(i) + 1; }
noInline(plusOne);
function isSlash(s, i) { return s.charCodeAt(i) === 47; }
noInline(isSlash);
function isNaNResult(s, i) { return Number.isNaN(s.charCodeAt(i)); }
noInline(isNaNResult);
function switchOn(s, i) {
    switch (s.charCodeAt(i)) {
    case 47:
        return 1;
    case 97:
        return 2;
    default:
        return 0;
    }
}
noInline(switchOn);
function rope(a, b, i) { return (a + b).charCodeAt(i); }
noInline(rope);

let string = "a/b";
for (let i = 0; i < testLoopCount; i++) {
    let index = i % 5;
    let code = index < 3 ? [97, 47, 98][index] : NaN;
    shouldBe(bitOr(string, index), Number.isNaN(code) ? 0 : code);
    shouldBe(bitAnd(string, index), Number.isNaN(code) ? 0 : code & 0xff);
    shouldBe(plusOne(string, index), code + 1);
    shouldBe(isSlash(string, index), code === 47);
    shouldBe(isNaNResult(string, index), Number.isNaN(code));
    shouldBe(switchOn(string, index), code === 47 ? 1 : (code === 97 ? 2 : 0));
    shouldBe(rope("a/", "b", index), code);
}

function anyIndex(s, i) { return s.charCodeAt(i); }
noInline(anyIndex);
for (let i = 0; i < testLoopCount; i++)
    shouldBe(anyIndex(string, i % 5), i % 5 < 3 ? [97, 47, 98][i % 5] : NaN);
shouldBe(anyIndex(string, Infinity), NaN);
shouldBe(anyIndex(string, -Infinity), NaN);
shouldBe(anyIndex(string, "1"), 47);
shouldBe(anyIndex(string, 1.9), 47);
shouldBe(anyIndex(string, NaN), 97);
