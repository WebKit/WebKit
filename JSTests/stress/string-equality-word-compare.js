function eq(a, b) { return a === b; }
noInline(eq);

function make(s) {
    // Force a fresh, resolved, non-atom JSString with its own StringImpl.
    return (s + "!").slice(0, s.length);
}

let cases = [];
for (let len = 0; len <= 40; ++len) {
    let base = "";
    for (let i = 0; i < len; ++i)
        base += String.fromCharCode(97 + (i % 26));
    cases.push([make(base), make(base), true]);
    if (len > 0) {
        for (let pos of [0, (len >> 1), len - 1]) {
            let arr = base.split("");
            arr[pos] = String.fromCharCode(arr[pos].charCodeAt(0) + 1);
            cases.push([make(base), make(arr.join("")), false]);
        }
    }
}

for (let i = 0; i < 1e4; ++i) {
    for (let [a, b, expected] of cases) {
        if (eq(a, b) !== expected)
            throw new Error("FAIL len=" + a.length + " a=" + a + " b=" + b + " expected=" + expected);
    }
}
