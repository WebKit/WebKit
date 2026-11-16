function eq(a, b) { return a === b; }
noInline(eq);

function make(s) {
    // Force a fresh, resolved, non-atom JSString with its own StringImpl.
    return (s + "α").slice(0, s.length);
}

// Build 16-bit strings (each character is non-Latin1 so storage is forced to 16-bit).
// Use a small alphabet of distinct non-Latin1 code points.
let alphabet = [];
for (let i = 0; i < 26; ++i)
    alphabet.push(String.fromCharCode(0x0391 + i)); // Greek capital letters

let cases = [];
for (let len = 0; len <= 40; ++len) {
    let base = "";
    for (let i = 0; i < len; ++i)
        base += alphabet[i % alphabet.length];
    cases.push([make(base), make(base), true]);
    if (len > 0) {
        for (let pos of [0, (len >> 1), len - 1]) {
            let arr = base.split("");
            arr[pos] = String.fromCharCode(arr[pos].charCodeAt(0) + 1);
            cases.push([make(base), make(arr.join("")), false]);
        }
    }
}

// Mixed-width pairs: same content but one side is 8-bit, the other 16-bit.
// These must compare unequal because we treat differing widths as a slow-path
// bail (the runtime helper handles equality across widths correctly).
let ascii = "abcdefghij";
let widened = ascii + "α";
let widenedTrimmed = widened.slice(0, ascii.length);
cases.push([ascii, widenedTrimmed, true]);

for (let i = 0; i < 1e4; ++i) {
    for (let [a, b, expected] of cases) {
        if (eq(a, b) !== expected)
            throw new Error("FAIL len=" + a.length + " a=" + a + " b=" + b + " expected=" + expected);
    }
}
