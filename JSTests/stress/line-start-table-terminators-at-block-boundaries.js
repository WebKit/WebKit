// Error positions come from the source's line start table, which is built in fixed-size blocks.
// Shifting every terminator kind across block boundaries by padding the first line checks that no
// boundary drops, duplicates, or misplaces a line start, including a CRLF split between two blocks.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

const wideMarker = String.fromCharCode(0x3042);
const lineSeparator = String.fromCharCode(0x2028);
const paragraphSeparator = String.fromCharCode(0x2029);
const recordStatement = "recorded.push(new Error());";
const throwStatement = "  throw new Error();";
const recorded = [];

function errorPosition(source) {
    try {
        eval(source);
    } catch (e) {
        return [e.line, e.column];
    }
    throw new Error("the source should have thrown");
}

const expectedThrowColumn = errorPosition(throwStatement)[1];
errorPosition(recordStatement + throwStatement);
const expectedRecordColumn = recorded.pop().column;

let seed = 1;
function nextLength() {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed % 90;
}
// Empty lines put several line starts in one block, so recorded lines also land at the third or
// later line start of a block.
const lineLengths = [];
for (let i = 0; i < 30; ++i)
    lineLengths.push(nextLength());

function check(terminators, wide, padding, name) {
    let source = "//" + (wide ? wideMarker : "") + "p".repeat(padding);
    const recordedLines = [];
    for (let i = 0; i < lineLengths.length; ++i) {
        source += terminators[i % terminators.length];
        if (lineLengths[i] < 30)
            continue;
        source += recordStatement + "//" + "x".repeat(lineLengths[i] - 30);
        recordedLines.push(i + 2);
    }
    source += terminators[0] + throwStatement;
    recorded.length = 0;
    const [line, column] = errorPosition(source);
    shouldBe(recorded.length, recordedLines.length, `recorded errors of ${name}, padding ${padding}`);
    for (let i = 0; i < recorded.length; ++i) {
        shouldBe(recorded[i].line, recordedLines[i], `line ${recordedLines[i]} of ${name}, padding ${padding}`);
        shouldBe(recorded[i].column, expectedRecordColumn, `column on line ${recordedLines[i]} of ${name}, padding ${padding}`);
    }
    shouldBe(line, lineLengths.length + 2, `line of the throw in ${name}, padding ${padding}`);
    shouldBe(column, expectedThrowColumn, `column of the throw in ${name}, padding ${padding}`);
}

// Lines can be empty, so a lone CR must never be followed by a terminator starting with LF: the two
// would merge into one CRLF.
const narrowKinds = [
    ["LF", ["\n"]],
    ["CR", ["\r"]],
    ["CRLF", ["\r\n"]],
    ["mixed", ["\r\n", "\n", "\r", "\r\n", "\r"]],
];
const wideKinds = narrowKinds.concat([
    ["LS", [lineSeparator]],
    ["PS", [paragraphSeparator]],
    ["mixed wide", [lineSeparator, "\r\n", paragraphSeparator, "\n", "\r"]],
]);

// A block is 64 Latin-1 characters or 32 UTF-16 code units; padding past one block adds nothing.
for (let padding = 0; padding < 66; ++padding) {
    for (const [name, terminators] of narrowKinds)
        check(terminators, false, padding, `8-bit ${name}`);
    if (padding < 34) {
        for (const [name, terminators] of wideKinds)
            check(terminators, true, padding, `16-bit ${name}`);
    }
}

// Long runs of terminators fill whole blocks, and enough of them overflow the scan's staging buffer.
for (const wide of [false, true]) {
    for (const terminator of ["\n", "\r", "\r\n"]) {
        const count = 1100;
        const source = "//" + (wide ? wideMarker : "") + terminator.repeat(count) + throwStatement;
        const [line, column] = errorPosition(source);
        const name = `${count} ${JSON.stringify(terminator)} in a ${wide ? "16" : "8"}-bit source`;
        shouldBe(line, count + 1, `line after ${name}`);
        shouldBe(column, expectedThrowColumn, `column after ${name}`);
    }
}
