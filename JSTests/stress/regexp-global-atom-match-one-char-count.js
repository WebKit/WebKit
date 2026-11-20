//@ runDefault

function shouldBe(actual, expected, description) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected + " (" + description + ")");
}

function numberOfMatches(string, regexp) {
    let result = string.match(regexp);
    return result === null ? 0 : result.length;
}

// The string is a repetition of unit, where "#" is the character to count and "." is the other one.
function countExpected(unit, end) {
    let inUnit = unit.split("#").length - 1;
    let inRemainder = unit.substring(0, end % unit.length).split("#").length - 1;
    return Math.floor(end / unit.length) * inUnit + inRemainder;
}

const units = [".", "#", ".#..", "#................", "#..#.#...##..#.#......#"];

const cases = [
    { character: "a", other: "z", regexp: /a/g },
    { character: "\xff", other: "\x7f", regexp: /\xff/g },
    { character: "a", other: "\u3042", regexp: /a/g },
    { character: "\uffff", other: "\u7fff", regexp: /\uffff/g },
];

// 16320 is 255 * 64, where the 8-bit lanes of the counters would overflow.
const lengths = [1, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 16319, 16320, 16321, 32640 + 63];

for (const { character, other, regexp } of cases) {
    for (const unit of units) {
        let block = unit.split("").map(c => c === "#" ? character : other).join("");
        for (const length of lengths) {
            let string = block.repeat(Math.floor(length / block.length)) + block.substring(0, length % block.length);
            let description = "U+" + character.charCodeAt(0).toString(16) + ", length " + length + ", unit " + unit;
            shouldBe(numberOfMatches(string, regexp), countExpected(unit, length), description);

            // Growing substrings of the same string resume counting from the previous end.
            for (const end of [Math.max(1, length >> 1), length]) {
                let expected = countExpected(unit, end) - countExpected(unit, 1);
                shouldBe(numberOfMatches(string.substring(1, end), regexp), expected, description + ", substring to " + end);
            }
        }
    }
}

// A single match must be counted once wherever it is in the vector blocks and in the tail.
for (const other of ["z", "\u3042"]) {
    for (let position = 0; position <= 80; ++position) {
        let string = other.repeat(position) + "a" + other.repeat(80 - position);
        shouldBe(numberOfMatches(string, /a/g), 1, "single match at " + position);
    }
}

// 65535 * 32 is where the 16-bit lanes of the counters would overflow.
{
    let length = 65535 * 32 + 33;
    shouldBe(numberOfMatches("\u3042".repeat(length), /\u3042/g), length, "16-bit, all matches");
}
