// Exhaustively verify that JSON.stringify produces correct output for every
// code unit, with the special character placed at every offset within a
// 16-byte SIMD stride and across stride boundaries. This guards the SIMD
// fast path in WTF::appendEscapedJSONStringContent.

function shouldBe(actual, expected, message)
{
    if (actual !== expected)
        throw new Error("FAIL: " + message + "\n  expected: " + expected + "\n  actual:   " + actual);
}

function needsEscape(code)
{
    return code < 0x20 || code === 0x22 || code === 0x5C;
}

function isSurrogate(code)
{
    return code >= 0xD800 && code <= 0xDFFF;
}

function expectedSingle(code)
{
    if (code === 0x08) return '\\b';
    if (code === 0x09) return '\\t';
    if (code === 0x0A) return '\\n';
    if (code === 0x0C) return '\\f';
    if (code === 0x0D) return '\\r';
    if (code === 0x22) return '\\"';
    if (code === 0x5C) return '\\\\';
    if (code < 0x20 || isSurrogate(code))
        return "\\u" + code.toString(16).padStart(4, "0");
    return String.fromCharCode(code);
}

const stride = 16;
const pad8 = "a";
const pad16 = "\u3042";

// 1. Every Latin1 code unit at every offset in [0, 2*stride], with 8-bit padding.
for (let code = 0; code <= 0xFF; ++code) {
    const ch = String.fromCharCode(code);
    const exp = expectedSingle(code);
    for (let prefix = 0; prefix <= 2 * stride + 1; ++prefix) {
        for (let suffix of [0, 1, stride - 1, stride, stride + 1]) {
            const s = pad8.repeat(prefix) + ch + pad8.repeat(suffix);
            const expected = '"' + pad8.repeat(prefix) + exp + pad8.repeat(suffix) + '"';
            shouldBe(JSON.stringify(s), expected, "latin1 code=" + code + " prefix=" + prefix + " suffix=" + suffix);
            if (!isSurrogate(code))
                shouldBe(JSON.parse(JSON.stringify(s)), s, "round-trip code=" + code);
        }
    }
}

// 2. Boundary code units for the 16-bit path (around surrogate range and BMP edges).
for (let code of [0x0100, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x5B, 0x5C, 0x5D, 0x7F, 0x80, 0xFF, 0xD7FE, 0xD7FF, 0xD800, 0xD801, 0xDBFF, 0xDC00, 0xDFFE, 0xDFFF, 0xE000, 0xE001, 0xFFFE, 0xFFFF]) {
    const ch = String.fromCharCode(code);
    const exp = expectedSingle(code);
    for (let prefix = 0; prefix <= 2 * stride + 1; ++prefix) {
        const s = pad16.repeat(prefix) + ch + pad16.repeat(stride);
        const expected = '"' + pad16.repeat(prefix) + exp + pad16.repeat(stride) + '"';
        shouldBe(JSON.stringify(s), expected, "16bit code=0x" + code.toString(16) + " prefix=" + prefix);
    }
}

// 3. Valid surrogate pairs at every offset must be passed through unescaped.
{
    const pair = "\uD83D\uDE00";
    for (let prefix = 0; prefix <= 2 * stride + 1; ++prefix) {
        const s = pad16.repeat(prefix) + pair + pad16.repeat(stride);
        shouldBe(JSON.stringify(s), '"' + s + '"', "valid surrogate pair prefix=" + prefix);
        shouldBe(JSON.parse(JSON.stringify(s)), s, "valid surrogate pair round-trip prefix=" + prefix);
    }
}

// 4. Escape characters straddling a stride boundary, including runs.
for (let len of [1, 2, stride - 1, stride, stride + 1, 2 * stride]) {
    for (let prefix of [0, 1, stride - 1, stride, stride + 1]) {
        const s = pad8.repeat(prefix) + "\n".repeat(len) + pad8.repeat(stride);
        const expected = '"' + pad8.repeat(prefix) + "\\n".repeat(len) + pad8.repeat(stride) + '"';
        shouldBe(JSON.stringify(s), expected, "run len=" + len + " prefix=" + prefix);
    }
}

// 5. Two escape characters separated by exactly N clean bytes (tests resume-after-escape).
for (let gap = 0; gap <= 2 * stride + 1; ++gap) {
    const s = pad8.repeat(stride) + "\n" + pad8.repeat(gap) + '"' + pad8.repeat(stride);
    const expected = '"' + pad8.repeat(stride) + "\\n" + pad8.repeat(gap) + '\\"' + pad8.repeat(stride) + '"';
    shouldBe(JSON.stringify(s), expected, "gap=" + gap);
}

// 6. Idempotency under repeated stringify on the same input (guards any per-string caching).
{
    const s = pad8.repeat(100) + "\n" + pad8.repeat(100);
    const first = JSON.stringify(s);
    for (let i = 0; i < 3; ++i)
        shouldBe(JSON.stringify(s), first, "idempotent");
}
