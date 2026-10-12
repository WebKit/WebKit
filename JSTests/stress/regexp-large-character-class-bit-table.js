//@ runDefault

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message()}: expected ${expected} but got ${actual}`);
}

function hex(codePoint) {
    return codePoint.toString(16);
}

function makeRanges(begin, end, step, widthOf) {
    const ranges = [];
    for (let codePoint = begin; codePoint + widthOf(codePoint) < end; codePoint += step)
        ranges.push([codePoint, codePoint + widthOf(codePoint)]);
    return ranges;
}

function classSource(ranges, unicode) {
    const escape = unicode ? (codePoint) => `\\u{${hex(codePoint)}}` : (codePoint) => `\\u${hex(codePoint).padStart(4, "0")}`;
    return ranges.map(([begin, end]) => begin === end ? escape(begin) : `${escape(begin)}-${escape(end)}`).join("");
}

// The code points around the ends of each range and around the multiples of 64 next to them, plus evenly spaced samples.
function codePointsToCheck(ranges) {
    const result = new Set([0, 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000, 0xffff, 0x10000, 0x1ffff, 0x20000, 0x10ffff]);
    const add = (codePoint) => {
        if (codePoint >= 0 && codePoint < 0x110000)
            result.add(codePoint);
    };
    for (const [begin, end] of ranges) {
        for (const codePoint of [begin, end]) {
            for (let delta = -1; delta <= 1; ++delta) {
                add(codePoint + delta);
                add((codePoint & ~63) + delta);
                add((codePoint | 63) + delta);
            }
        }
    }
    for (let codePoint = 0; codePoint < 0x110000; codePoint += 0x3f7)
        add(codePoint);
    return Array.from(result).sort((a, b) => a - b);
}

class TestClass {
    constructor(ranges) {
        if (ranges.length < 128)
            throw new Error("The class is too small to be tested with a bit table");
        this.ranges = ranges;
        this.members = new Uint8Array(0x110000);
        for (const [begin, end] of ranges)
            this.members.fill(1, begin, end + 1);
        this.codePoints = codePointsToCheck(ranges);
    }

    source(unicode) {
        return classSource(this.ranges, unicode);
    }

    check(regExp, expectMember, stride = 1) {
        const limit = regExp.unicode || regExp.unicodeSets ? 0x110000 : 0x10000;
        for (let i = 0; i < this.codePoints.length; i += stride) {
            const codePoint = this.codePoints[i];
            if (codePoint >= limit)
                break;
            shouldBe(regExp.test(String.fromCodePoint(codePoint)), !!this.members[codePoint] === expectMember, () => `${regExp.flags} U+${hex(codePoint)}`);
        }
    }
}

// Ranges in the BMP, the SMP and above, including ones crossing 64 code point boundaries, a run of
// full words, a range crossing U+20000 and a range ending at U+10FFFF.
const mixed = new TestClass([
    ...makeRanges(0x80, 0x3000, 0x29, (codePoint) => codePoint % 7),
    [0x3000, 0x30ff],
    [0x3140, 0x317f],
    ...makeRanges(0x4000, 0xd000, 0x1c7, (codePoint) => codePoint % 131),
    [0xd7f0, 0xd805],
    [0xdbff, 0xdc00],
    [0xdfff, 0xe010],
    [0xfffe, 0x10001],
    ...makeRanges(0x10100, 0x1f000, 0x3fb, (codePoint) => codePoint % 97),
    [0x1ffc0, 0x20040],
    ...makeRanges(0x21000, 0x30000, 0x2003, (codePoint) => codePoint % 1000),
    [0xe0100, 0xe01ef],
    [0x10fff0, 0x10ffff],
]);
mixed.check(new RegExp(`^[${mixed.source(true)}]$`, "u"), true);
mixed.check(new RegExp(`^[^${mixed.source(true)}]$`, "u"), false);
mixed.check(new RegExp(`^[${mixed.source(true)}]+$`, "v"), true, 8);
mixed.check(new RegExp(`(?<=^[${mixed.source(true)}])$`, "u"), true, 8);
mixed.check(new RegExp(`^[${mixed.source(true)}]*?$`, "u"), true, 8);

{
    const string = String.fromCodePoint(...mixed.codePoints.filter((codePoint) => codePoint < 0xd800 || codePoint > 0xdfff));
    const expected = Array.from(string).filter((character) => mixed.members[character.codePointAt(0)]).join("");
    shouldBe(string.match(new RegExp(`[${mixed.source(true)}]+`, "gu")).join(""), expected, () => "greedy runs");
}

// Without the u flag the character is a code unit, so surrogates are matched on their own.
const bmp = new TestClass(mixed.ranges.filter(([begin, end]) => end <= 0xffff));
bmp.check(new RegExp(`^[${bmp.source(false)}]$`), true);
bmp.check(new RegExp(`^[^${bmp.source(false)}]$`), false);
{
    const regExp = new RegExp(`^[${bmp.source(false)}]{2}$`);
    for (let codePoint = 0x10000; codePoint < 0x110000; codePoint += 0x3fd) {
        const string = String.fromCodePoint(codePoint);
        shouldBe(regExp.test(string), !!(bmp.members[string.charCodeAt(0)] && bmp.members[string.charCodeAt(1)]), () => `{2} U+${hex(codePoint)}`);
    }
}

// A class whose table covers every code unit.
const wholeBMP = new TestClass([...bmp.ranges, [0xffc1, 0xffc3], [0xfff0, 0xffff]]);
wholeBMP.check(new RegExp(`^[${wholeBMP.source(false)}]$`), true);
wholeBMP.check(new RegExp(`^[^${wholeBMP.source(false)}]$`), false);

// Latin-1 characters in a 16-bit string.
for (const [testClass, flags] of [[bmp, ""], [mixed, "u"]]) {
    const regExp = new RegExp(`^\\u3042[${testClass.source(flags === "u")}]$`, flags);
    for (let codePoint = 0; codePoint < 0x100; ++codePoint)
        shouldBe(regExp.test("\u3042" + String.fromCharCode(codePoint)), !!testClass.members[codePoint], () => `16-bit /${flags} U+${hex(codePoint)}`);
}

// A class whose code points all fit in a few words.
const dense = new TestClass(makeRanges(0x100, 0x300, 3, () => 1));
dense.check(new RegExp(`^[${dense.source(true)}]$`, "u"), true);
dense.check(new RegExp(`^[${dense.source(false)}]$`), true);

// Only single code points above U+20000.
const singlesAbove = new TestClass([...dense.ranges, [0x20000, 0x20000], [0x2abcd, 0x2abcd], [0x10ffff, 0x10ffff]]);
singlesAbove.check(new RegExp(`^[${singlesAbove.source(true)}]$`, "u"), true);
singlesAbove.check(new RegExp(`^[^${singlesAbove.source(true)}]$`, "u"), false);

// A class with nothing below U+20000.
const high = new TestClass(makeRanges(0x20000, 0x30000, 0x101, (codePoint) => codePoint % 50));
high.check(new RegExp(`^[${high.source(true)}]$`, "u"), true);

// Two classes under the same lead surrogate in one RegExp.
{
    const first = new TestClass(makeRanges(0x10000, 0x10400, 4, () => 1));
    const second = new TestClass(makeRanges(0x10002, 0x10400, 4, () => 1));
    const regExp = new RegExp(`^[${first.source(true)}][${second.source(true)}]$`, "u");
    for (let codePoint = 0x10000; codePoint < 0x10400; codePoint += 3) {
        for (const other of [0x10028, 0x1002a]) {
            const string = String.fromCodePoint(codePoint, other);
            shouldBe(regExp.test(string), !!(first.members[codePoint] && second.members[other]), () => `U+${hex(codePoint)} U+${hex(other)}`);
        }
    }
}

// Unicode properties.
function checkProperty(source, flags, members, nonMembers) {
    const regExp = new RegExp(`^${source}$`, flags);
    for (const codePoint of members)
        shouldBe(regExp.test(String.fromCodePoint(codePoint)), true, () => `${source}/${flags} U+${hex(codePoint)}`);
    for (const codePoint of nonMembers)
        shouldBe(regExp.test(String.fromCodePoint(codePoint)), false, () => `${source}/${flags} U+${hex(codePoint)}`);
}

const letters = [0x41, 0x7a, 0xaa, 0x3b1, 0x3042, 0x4e00, 0x9fff, 0xac00, 0xffdc, 0x10000, 0x1d7cb, 0x1eebb, 0x20000, 0x2a6df, 0x2f800, 0x30000, 0x3134a];
const nonLetters = [0x0, 0x20, 0x30, 0x40, 0x5b, 0xd7, 0x300, 0x3000, 0xd800, 0xdfff, 0xffff, 0x1000c, 0x1eebc, 0x1ffff, 0x2a6e0, 0x2fa1e, 0x3134b, 0xe0100, 0x10ffff];
checkProperty("\\p{L}", "u", letters, nonLetters);
checkProperty("\\p{L}+", "u", letters, nonLetters);
checkProperty("\\P{L}", "u", nonLetters, letters);
checkProperty("[^\\p{L}]", "u", nonLetters, letters);
checkProperty("\\p{L}", "v", letters, nonLetters);
checkProperty("[\\p{L}\\p{Nd}_]", "u", [...letters, 0x30, 0x5f, 0x660, 0x1d7ff], nonLetters.filter((codePoint) => codePoint !== 0x30));
checkProperty("[\\p{L}--\\p{Script=Han}]", "v", letters.filter((codePoint) => codePoint < 0x4e00 || (codePoint > 0x9fff && codePoint < 0x20000)), [...nonLetters, 0x4e00, 0x20000, 0x3134a]);
checkProperty("\\p{L}", "iu", letters, nonLetters);
checkProperty(".(?<=\\p{L})", "su", letters, nonLetters);
checkProperty("\\p{ID_Continue}", "u", [...letters, 0x30, 0x5f, 0x300, 0xe0100, 0xe01ef], [0x20, 0x40, 0xd800, 0xe00ff, 0xe01f0, 0x10ffff]);
checkProperty("\\p{Cn}", "u", [0x378, 0xffff, 0x1ffff, 0x2a6e0, 0x3347a, 0xe0000, 0xeffff], [0x41, 0xd800, 0xe000, 0x20000, 0xf0000, 0x10fffd]);

shouldBe("x1 日本語 𠮷野家 Ünïcödé".match(/\p{L}+/gu).join("|"), "x|日本語|𠮷野家|Ünïcödé", () => "match /gu");
