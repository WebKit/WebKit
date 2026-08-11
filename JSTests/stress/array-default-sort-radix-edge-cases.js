// Default (no-comparator) Array.prototype.sort must order elements like a comparator comparing
// String(a) vs String(b) in code-unit order. Comparing serialized results (not identity) keeps
// the legitimate instability of equal strings from causing false failures.

function ser(arr) {
    let parts = [];
    for (let i = 0; i < arr.length; i++)
        parts.push((i in arr) ? ("|" + String(arr[i])) : "|<hole>");
    return parts.join("");
}

function refCmp(a, b) {
    let sa = String(a), sb = String(b);
    return sa < sb ? -1 : (sa > sb ? 1 : 0);
}

// slice() preserves holes and undefined, so each operation gets an independent copy
// of the same input (objects are shared by reference, which is fine here).
function check(arr, label) {
    let a = arr.slice();
    let lenBefore = a.length;
    let sorted = a.sort();
    if (sorted !== a)
        throw new Error(label + ": sort() must return the same array");
    if (a.length !== lenBefore)
        throw new Error(label + ": length changed " + lenBefore + " -> " + a.length);
    let expected = arr.slice().sort(refCmp);
    if (ser(a) !== ser(expected))
        throw new Error(label + ": order mismatch\n  got: " + ser(a).slice(0, 200) + "\n  exp: " + ser(expected).slice(0, 200));
    // toSorted must not mutate the receiver, and must order like refCmp's toSorted.
    // (toSorted reads holes as undefined, unlike in-place sort which keeps trailing holes,
    // so it is compared against refCmp's toSorted rather than against sort.)
    let src = arr.slice();
    let copy = src.slice();
    let ts = src.toSorted();
    if (ser(src) !== ser(copy))
        throw new Error(label + ": toSorted mutated the receiver");
    let tsExpected = arr.slice().toSorted(refCmp);
    if (ser(ts) !== ser(tsExpected))
        throw new Error(label + ": toSorted mismatch\n  got: " + ser(ts).slice(0, 200) + "\n  exp: " + ser(tsExpected).slice(0, 200));
}

// --- Boundary around the radix threshold (14) and the depth cap (32 levels: 32 Latin-1 chars / 16 UTF-16 code units). ---
for (let n = 0; n <= 40; n++) {
    check(Array.from({ length: n }, (_, i) => (i * 2654435761) % 97), "numbers n=" + n);
    check(Array.from({ length: n }, (_, i) => "s" + ((i * 40503) % 13)), "shortstr n=" + n);
}

// --- All identical strings (everything funnels into one bucket, then terminal bucket 0). ---
for (let n of [1, 13, 14, 15, 100, 500]) {
    check(Array.from({ length: n }, () => "same"), "identical-short n=" + n);
    check(Array.from({ length: n }, () => "x".repeat(40)), "identical-deep n=" + n); // >32 chars -> depth cap
    check(Array.from({ length: n }, () => "\u3042".repeat(20)), "identical-deep-16bit n=" + n); // >16 code units -> depth cap
}

// --- Empty strings, and prefixes of varying length (terminal buckets at many depths). ---
check(["", "a", "ab", "abc", "a", "", "abcd", "ab", "abc", "abce", "abcz", "abca"], "prefix-ladder");
(() => {
    let a = [];
    let words = ["", "a", "aa", "aaa", "aaaa", "ab", "aba", "abb", "b", "ba", "bb", "z", ""];
    for (let i = 0; i < 50; i++) a.push(words[(i * 7) % words.length]);
    check(a, "prefix-ladder-big");
})();

// --- Code-unit boundaries: NUL (byte 0), 0xFF/0x100 (high/low byte split), 0xFFFF. ---
check(["\u0000", "\u0000\u0000", "", "\u0001", "\u0000a", "a", "a\u0000"], "nul-bytes");
(() => {
    let a = [];
    for (let i = 0; i < 60; i++) {
        // alternate code units that differ only in low byte vs only in high byte
        let cu = (i % 2) ? (0x0100 + (i % 5)) : (0x0001 + (i % 5) * 0x0100);
        a.push(String.fromCharCode(cu) + String.fromCharCode(0x00FF + (i % 3)));
    }
    check(a, "high-low-byte-split");
})();
(() => {
    let a = [];
    for (let i = 0; i < 64; i++)
        a.push(String.fromCharCode((i * 0x3B) & 0xFF, 0xFF - (i % 3), i % 2 ? 0x00 : 0xFF) + "x");
    a.push("\u0000", "\u00FF", "\u00FF\u00FF", "\u0000\u0000", "");
    check(a, "latin1-full-byte-range");
})();
check(["\uFFFF", "\uFFFE", "\u0100", "\u00FF", "\u0000", "\u0001", "\uFFFF\u0000", "\uFFFF"], "wide-code-units");

// --- Surrogate code units / non-BMP: default sort is code-unit order, not code-point order. ---
check(["\uD800", "\uDFFF", "\uD7FF", "\uE000", "\uD800\uDC00", "\uDBFF\uDFFF", "a", "\uFFFF"], "surrogates");
(() => {
    let a = [];
    for (let i = 0; i < 50; i++) a.push(String.fromCodePoint(0x1F600 + (i % 7)) + String.fromCharCode(97 + (i % 4)));
    check(a, "non-bmp");
})();

// --- Numbers stringified: negatives, decimals, -0, Infinity, NaN. ---
check([10, 9, 100, 1, 20, 2, 3, 30, 0, -1, -10, -2, 11, 21, 101, 110, 12, 13, 14, 15, 16, 17, 18, 19, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33], "number-string-order");
check([Infinity, -Infinity, NaN, 0, -0, 1.5, 1.25, 1.125, 1e21, 1e-7, 0.1, 0.2, 100, 99, 9, NaN, Infinity], "special-numbers");

// --- undefined and holes placement (must be: values, then undefined, then holes). ---
check([3, undefined, 1, undefined, 2], "undefined-mix");
(() => { let a = [5, 4, 3, 2, 1]; a[10] = 0; a[20] = 9; check(a, "sparse-holes"); })();
(() => { let a = new Array(40); for (let i = 0; i < 40; i += 2) a[i] = (i * 13) % 50; a[5] = undefined; a[7] = undefined; check(a, "holes-and-undefined-big"); })();

// --- Mixed types in one array (all funnel through ToString). ---
check([true, false, null, 1, "1", "true", {}, [1, 2], "a", 0, "0", "", "[object Object]"], "mixed-types");
(() => {
    let a = [];
    for (let i = 0; i < 60; i++) {
        let r = i % 6;
        a.push(r === 0 ? i : r === 1 ? String(i) : r === 2 ? (i % 2 === 0) : r === 3 ? { x: i } : r === 4 ? [i] : null);
    }
    check(a, "mixed-types-big");
})();

// --- Deep shared prefix beyond the depth cap (ordering must still be correct), 8-bit and 16-bit. ---
(() => {
    let a = [];
    let base = "Z".repeat(40);
    for (let i = 0; i < 80; i++) a.push(base + ((i * 31) % 17) + "tail");
    check(a, "deep-prefix-ordering");
    let b = [];
    let base16 = "\u3042".repeat(20);
    for (let i = 0; i < 80; i++) b.push(base16 + ((i * 31) % 17) + "tail");
    check(b, "deep-prefix-ordering-16bit");
})();

// --- A single 16-bit string switches the whole sort to the UTF-16 byte path. ---
(() => {
    let a = [];
    for (let i = 0; i < 80; i++) a.push("key" + ((i * 37) % 53));
    a.push("key\u0100", "key\u00FF", "ke\uFFFF");
    check(a, "one-16bit-among-8bit");
})();

// --- Randomized differential across alphabets and sizes (deterministic seed). ---
let seed = 0x9e3779b9 >>> 0;
function rnd() {
    seed ^= seed << 13; seed >>>= 0;
    seed ^= seed >> 17;
    seed ^= seed << 5; seed >>>= 0;
    return seed >>> 0;
}
for (let trial = 0; trial < testLoopCount / 10; trial++) {
    let n = rnd() % 400;
    let mode = rnd() % 5;
    let a = new Array(n);
    for (let i = 0; i < n; i++) {
        if (mode === 0) a[i] = rnd() % 1000000;                       // numbers, small distinct prefix
        else if (mode === 1) { let len = rnd() % 8, s = ""; for (let j = 0; j < len; j++) s += String.fromCharCode(97 + rnd() % 4); a[i] = s; } // tiny alphabet, many collisions
        else if (mode === 2) { let len = 1 + rnd() % 6, s = ""; for (let j = 0; j < len; j++) s += String.fromCharCode(32 + rnd() % 200); a[i] = s; } // wide alphabet, BMP
        else if (mode === 3) { let len = rnd() % 5, s = ""; for (let j = 0; j < len; j++) s += String.fromCharCode(0xD800 + rnd() % 0x800); a[i] = s; } // raw surrogate code units
        else { let r = rnd() % 4; a[i] = r === 0 ? (rnd() % 50) : r === 1 ? String.fromCharCode(97 + rnd() % 5) : r === 2 ? undefined : null; } // mixed incl undefined
    }
    check(a, "rand trial=" + trial + " mode=" + mode + " n=" + n);
}

// --- Stability where the radix path guarantees it (shallow keys, n >= 14). ---
// Distinct objects whose ToString collides on short keys keep insertion order: equal-key
// runs are scattered stably and identical strings land in the terminal bucket in order.
function checkStability(n, distinctKeys, label) {
    let a = [];
    for (let i = 0; i < n; i++) {
        let key = "k" + (rnd() % distinctKeys);
        let o = { id: i, key };
        o.toString = function () { return this.key; };
        a.push(o);
    }
    let expected = a.slice().sort((x, y) => x.key < y.key ? -1 : x.key > y.key ? 1 : x.id - y.id);
    let got = a.slice().sort();
    for (let i = 0; i < n; i++) {
        if (got[i] !== expected[i])
            throw new Error("stability mismatch [" + label + "] at i=" + i);
    }
}
for (let trial = 0; trial < testLoopCount / 20; trial++)
    checkStability(14 + rnd() % 400, 1 + rnd() % 8, "stab trial=" + trial);
