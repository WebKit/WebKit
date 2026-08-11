// Exhaustively verifies backtracking of a greedy variable-width character class in
// unicode mode against a reference implementation, over every string of up to 5
// characters drawn from an alphabet covering all code unit width combinations:
// BMP characters, a lone lead surrogate, a lone trail surrogate, and a surrogate pair.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${expected}, got ${actual}`);
}

// Reference for new RegExp("([^<excluded>]*)<literal>", "u").exec(subject):
// leftmost start position (in code points), then greedy longest body first.
function referenceExec(subject, excluded, literal) {
    let cps = [...subject]; // code point segmentation, as in unicode mode
    let unitOffsets = [];
    let offset = 0;
    for (let cp of cps) {
        unitOffsets.push(offset);
        offset += cp.length;
    }
    unitOffsets.push(offset);

    let literalCps = [...literal];
    for (let start = 0; start <= cps.length; start++) {
        let max = 0;
        while (start + max < cps.length && !excluded.includes(cps[start + max]))
            max++;
        for (let count = max; count >= 0; count--) {
            let literalStart = start + count;
            if (literalStart + literalCps.length > cps.length)
                continue;
            let matched = true;
            for (let i = 0; i < literalCps.length; i++) {
                if (cps[literalStart + i] !== literalCps[i]) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                let body = cps.slice(start, start + count).join("");
                return { match: body + literal, body, index: unitOffsets[start] };
            }
        }
    }
    return null;
}

let lead = "\uD800";
let trail = "\uDC00";
let np = lead + trail; // U+10000, one code point, two code units

const alphabet = ["a", "\"", "X", lead, trail, np];
const maxLength = 5;

const cases = [
    { re: /([^"]*)X/u, excluded: ["\""], literal: "X" },
    { re: new RegExp("([^\"]*)" + np, "u"), excluded: ["\""], literal: np },
    { re: new RegExp("([^X]*)" + lead, "u"), excluded: ["X"], literal: lead },
    { re: new RegExp("([^X]*)" + trail, "u"), excluded: ["X"], literal: trail },
    { re: new RegExp("([^X" + np + "]*)a", "u"), excluded: ["X", np], literal: "a" },
];

function checkSubject(subject) {
    for (let i = 0; i < cases.length; i++) {
        let { re, excluded, literal } = cases[i];
        let expected = referenceExec(subject, excluded, literal);

        let actual = re.exec(subject);
        if (expected === null) {
            shouldBe(actual, null, `exec /${re.source}/u on ${JSON.stringify(subject)}`);
            shouldBe(re.test(subject), false, `test /${re.source}/u on ${JSON.stringify(subject)}`);
            continue;
        }
        shouldBe(actual === null, false, `exec /${re.source}/u on ${JSON.stringify(subject)}`);
        shouldBe(actual[0], expected.match, `match /${re.source}/u on ${JSON.stringify(subject)}`);
        shouldBe(actual[1], expected.body, `capture /${re.source}/u on ${JSON.stringify(subject)}`);
        shouldBe(actual.index, expected.index, `index /${re.source}/u on ${JSON.stringify(subject)}`);
        shouldBe(re.test(subject), true, `test /${re.source}/u on ${JSON.stringify(subject)}`);
    }
}

function enumerate(prefix, length) {
    checkSubject(prefix);
    if (length === maxLength)
        return;
    for (let atom of alphabet)
        enumerate(prefix + atom, length + 1);
}

enumerate("", 0);
