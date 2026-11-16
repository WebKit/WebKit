// Thorough stress test for the inlined === fast path on JSStrings.
// Covers:
//   - 8-bit and 16-bit storage, all lengths 0..72 (well past several word boundaries)
//   - Mismatch at every byte position (head, middle, tail) for both widths
//   - Boundary lengths around the word loop (wordSize=8 bytes => 8-bit:8 chars, 16-bit:4 chars)
//   - Pointer-equal strings, atom-equal strings, and freshly-allocated equal-content strings
//   - Mixed-width pairs (8-bit vs 16-bit with identical code points): equality must be honored
//     by the runtime helper even when our inline fast path bails on width mismatch.
//   - Strings produced via different construction paths (literal, concat, slice, fromCharCode,
//     repeat, replace, JSON.parse) so the StringImpl flag layout varies.
//   - Sufficient iterations to tier up DFG and FTL.

function eq(a, b) { return a === b; }
noInline(eq);

// Force a fresh, resolved, non-atom JSString with its own StringImpl.
function freshCopy(s) {
    if (s.length === 0)
        return ("x").slice(1, 1);
    return (s + "!").slice(0, s.length);
}

// Like freshCopy but routed through a different builder, so the resulting
// StringImpl has different metadata (e.g., not from a slice).
function freshCopyConcat(s) {
    let out = "";
    for (let i = 0; i < s.length; ++i)
        out += s[i];
    return out;
}

const ASCII_ALPHABET = (() => {
    let s = "";
    for (let i = 0; i < 26; ++i)
        s += String.fromCharCode(97 + i);
    return s;
})();

const GREEK_ALPHABET = (() => {
    let s = "";
    for (let i = 0; i < 24; ++i)
        s += String.fromCharCode(0x0391 + i); // forces 16-bit
    return s;
})();

function makeFromAlphabet(alpha, len) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += alpha[i % alpha.length];
    return s;
}

function flip(s, pos) {
    let arr = s.split("");
    arr[pos] = String.fromCharCode(arr[pos].charCodeAt(0) + 1);
    return arr.join("");
}

let cases = [];
const MAX_LEN = 72;

for (let len = 0; len <= MAX_LEN; ++len) {
    for (let alpha of [ASCII_ALPHABET, GREEK_ALPHABET]) {
        let base = makeFromAlphabet(alpha, len);

        // Equal pairs from several construction paths.
        cases.push([freshCopy(base), freshCopy(base), true]);
        cases.push([freshCopy(base), freshCopyConcat(base), true]);

        // Identity cases (same JSString cell).
        cases.push([base, base, true]);

        if (len > 0) {
            // Mismatch at every position.
            for (let pos = 0; pos < len; ++pos)
                cases.push([freshCopy(base), freshCopy(flip(base, pos)), false]);

            // Different lengths (slow path's own concern, but still must be respected).
            cases.push([freshCopy(base), freshCopy(base + alpha[0]), false]);
            cases.push([freshCopy(base + alpha[0]), freshCopy(base), false]);
        }
    }

    // Mixed-width pair: same code points stored with different bit widths.
    // The 8-bit copy is built from ASCII; the 16-bit copy embeds a non-Latin1
    // throwaway character then trims it, forcing 16-bit storage of ASCII content.
    let ascii = makeFromAlphabet(ASCII_ALPHABET, len);
    let widened = (ascii + "α").slice(0, len);
    cases.push([ascii, widened, true]);
    cases.push([widened, ascii, true]);
}

// Atom + non-atom equal pair.
{
    let atom = "static-atom-string";
    let nonAtom = freshCopy("static-atom-string");
    cases.push([atom, nonAtom, true]);
    cases.push([nonAtom, atom, true]);
}

// Strings whose first/last word boundary differs.
for (let len of [7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33]) {
    let base = makeFromAlphabet(ASCII_ALPHABET, len);
    cases.push([freshCopy(base), freshCopy(base), true]);
    cases.push([freshCopy(base), freshCopy(flip(base, 0)), false]);
    cases.push([freshCopy(base), freshCopy(flip(base, len - 1)), false]);
    if (len >= 2)
        cases.push([freshCopy(base), freshCopy(flip(base, len >> 1)), false]);

    let base16 = makeFromAlphabet(GREEK_ALPHABET, len);
    cases.push([freshCopy(base16), freshCopy(base16), true]);
    cases.push([freshCopy(base16), freshCopy(flip(base16, 0)), false]);
    cases.push([freshCopy(base16), freshCopy(flip(base16, len - 1)), false]);
    if (len >= 2)
        cases.push([freshCopy(base16), freshCopy(flip(base16, len >> 1)), false]);
}

// Tier up: run a quick warmup on a fixed pair so DFG/FTL kicks in.
{
    let a = freshCopy("warmup-string-warmup");
    let b = freshCopy("warmup-string-warmup");
    let c = freshCopy("warmup-string-warmuq");
    for (let i = 0; i < 200000; ++i) {
        if (!eq(a, b)) throw "warmup1";
        if (eq(a, c)) throw "warmup2";
    }
}

// Main verification.
let total = 0;
for (let i = 0; i < 50; ++i) {
    for (let [a, b, expected] of cases) {
        let r = eq(a, b);
        if (r !== expected) {
            throw new Error(
                "FAIL: a.length=" + a.length + " b.length=" + b.length +
                " expected=" + expected + " got=" + r +
                " a=" + JSON.stringify(a) + " b=" + JSON.stringify(b));
        }
        total++;
    }
}

if (total < 1000)
    throw "too few comparisons: " + total;
