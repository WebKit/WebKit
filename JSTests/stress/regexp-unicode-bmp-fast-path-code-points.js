//@ memoryHog!
function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`FAIL ${message}: expected ${expected}, got ${actual}`);
}

const codeUnits = [];
for (let unit = 0; unit < 0x10000; unit += 0x40)
    codeUnits.push(unit);
for (const unit of [0x03ff, 0x0400, 0x2014, 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000, 0xfffd, 0xffff])
    codeUnits.push(unit);

const followers = [0x0061, 0x0400, 0x3042, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xffff];

function codePointsOf(string) {
    const result = [];
    for (const character of string)
        result.push(character.codePointAt(0));
    return result;
}

const singleCodePoint = /^.$/u;
const twoCodePoints = /^(.)(.)$/u;
const greedy = /^([^X]*)X$/u;

for (const first of codeUnits) {
    for (const second of followers) {
        const subject = String.fromCharCode(first, second);
        const expected = codePointsOf(subject);
        const tag = `[${first.toString(16)},${second.toString(16)}]`;

        shouldBe(singleCodePoint.test(subject), expected.length === 1, `single code point ${tag}`);

        const matched = twoCodePoints.exec(subject);
        shouldBe(matched !== null, expected.length === 2, `two code points ${tag}`);
        if (matched) {
            shouldBe(matched[1].codePointAt(0), expected[0], `first code point ${tag}`);
            shouldBe(matched[2].codePointAt(0), expected[1], `second code point ${tag}`);
        }

        const scanned = greedy.exec(subject + "X");
        shouldBe(scanned !== null, true, `greedy scan ${tag}`);
        shouldBe(scanned[1], subject, `greedy capture ${tag}`);
    }
}
