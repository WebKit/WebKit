function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

function flatten(string) {
    return string.split("").join("");
}
noInline(flatten);

function replace(string, pattern, replacement) {
    return string.replace(pattern, replacement);
}
noInline(replace);

function check(base, start, end, pattern, replacement) {
    let flat = flatten(base.slice(start, end));
    let expected = replace(flat, pattern, replacement);
    let substring = base.slice(start, end);
    shouldBe(replace(substring, pattern, replacement), expected);
    shouldBe(replace(substring, pattern, replacement), expected);
    shouldBe(substring, flat);
    shouldBe(replace(base.slice(0, end + 1).slice(start, end), pattern, replacement), expected);
}
noInline(check);

let latin1 = "  the <quick> brown & 'lazy' \"fox\" jumps 12px over\r\n34px dogs, then rests   ";
let wide = "  the <quick> \u65e5\u672c\u8a9e & 'lazy' \"fox\" \ud83d\ude80 jumps 12px over\r\n34px \u4e16\u754c, then rests   ";
let plain = "the quick brown fox jumps over the lazy dog and then rests for a while under a tree";
let plainWide = "the quick \u65e5\u672c\u8a9e fox jumps over the \ud83d\ude80 dog and then rests for a while under a tree";

let escape = (character) => `&#${character.charCodeAt(0)};`;
let cases = [
    [/[&<>"']/g, escape],
    [/[&<>"']/, escape],
    [/(\d+)(px)/g, (match, digits, unit, offset, whole) => `${digits}:${unit}:${offset}:${whole.length}`],
    [/(?<digits>\d+)px/g, (match, digits, offset, whole, groups) => `${groups.digits}em`],
    [/\r\n/g, " "],
    [/[\r\n]+/g, " "],
    [/ /g, "_"],
    [/(\d+)px/g, "$1em"],
    [/(\d+)px/g, "[$&|$`|$']"],
    [/(?<digits>\d+)px/g, "$<digits>em"],
    [/(\d+)px/, "$1em"],
    [/quick/, "slow"],
    [/quick/, ""],
    [/quick/g, ""],
    [/quick/g, "slow"],
    [/quick/g, 42],
    [/quick/g, "[$&]"],
    [/^\s+/g, ""],
    [/\s+$/g, ""],
    [/rests\s*$/, ""],
    [/[aeiou]/g, ""],
    [/\s+/g, ""],
    [/^\s+/, ""],
    [/\s+$/, ""],
    [/^\s*/, ""],
    [/\s*$/, ""],
    [/^ +the/, ""],
    [/x*/g, "-"],
    [/x*/gu, "-"],
    [/x*/gv, "-"],
    [/x*/gu, "[$&]"],
    [/x*/gu, ""],
    [/x*/gu, Math.abs],
    [/(?:)/gu, (match, offset) => offset % 7 ? "" : "|"],
    [/\p{Script=Han}+/gu, "(han)"],
    [/zzz/g, "never"],
    [/zzz/, "never"],
    [/zzz/g, ""],
    [/zzz/, ""],
    [/zzz/g, escape],
    [/zzz/, escape],
    [/z(z)z/g, "$1"],
];

for (let [pattern, replacement] of cases) {
    for (let base of [latin1, wide, plain, plainWide]) {
        check(base, 0, 40, pattern, replacement);
        check(base, 2, base.length - 3, pattern, replacement);
        check(base, 17, 60, pattern, replacement);
        check(base, 17, base.length - 1, pattern, replacement);
        check(base, 5, 7, pattern, replacement);
    }
    for (let base of [wide, plainWide])
        check(base, 3, base.indexOf("\ud83d\ude80") + 1, pattern, replacement);
}

function removeGlobal(string) {
    return string.replace(/[\r\n]+/g, "");
}
noInline(removeGlobal);

function removeFirst(string) {
    return string.replace(/^\s+/, "");
}
noInline(removeFirst);

function removeAll(string) {
    return string.replaceAll(/o/g, "");
}
noInline(removeAll);

function replaceWithString(string) {
    return string.replace(/ +/g, "_");
}
noInline(replaceWithString);

function replaceWithFunction(string) {
    return string.replace(/[&<>"']/g, escape);
}
noInline(replaceWithFunction);

let hot = [removeGlobal, removeFirst, removeAll, replaceWithString, replaceWithFunction];
let inputs = [];
for (let base of [latin1, wide, plain, plainWide]) {
    for (let [start, end] of [[0, 40], [17, 60]])
        inputs.push({ base, start, end, expected: hot.map((operation) => operation(flatten(base.slice(start, end)))) });
}

for (let i = 0; i < testLoopCount; ++i) {
    let { base, start, end, expected } = inputs[i % inputs.length];
    for (let j = 0; j < hot.length; ++j)
        shouldBe(hot[j](base.slice(start, end)), expected[j]);
}

function replaceAndAtomize(base, twin) {
    let keys = {};
    keys[twin] = 1;
    let substring = base.slice(3, base.length - 3);
    let calls = 0;
    let result = substring.replace(/[&<>"']/g, (character, offset, whole) => {
        if (!calls++) {
            keys[base] = 2;
            keys[whole] = 3;
            keys[flatten(base + offset)] = 4;
            gc();
        }
        return escape(character);
    });
    shouldBe(result, flatten(base).slice(3, base.length - 3).replace(/[&<>"']/g, escape));
    shouldBe(substring, flatten(base).slice(3, base.length - 3));
    shouldBe(Object.keys(keys).length, 3);
}
noInline(replaceAndAtomize);

for (let i = 0; i < 2; ++i) {
    replaceAndAtomize(flatten(latin1 + i), flatten(latin1 + i));
    replaceAndAtomize(flatten(wide + i), flatten(wide + i));
}

let long = (latin1 + wide).repeat(60);
let longExpected = flatten(long.slice(1, long.length - 1)).replace(/[&<>"']/g, escape);
for (let i = 0; i < 4; ++i)
    shouldBe(long.slice(1, long.length - 1).replace(/[&<>"']/g, escape), longExpected);
