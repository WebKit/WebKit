//@ runDefault

// Deep rope construction
function makeDeepRope(n) {
    let s = '';
    for (let i = 0; i < n; i++)
        s += String.fromCharCode(65 + (i % 26)); // 'A'-'Z' cycling
    return s;
}

// Build a reference flat string for comparison
function makeFlat(n) {
    let chars = [];
    for (let i = 0; i < n; i++)
        chars.push(String.fromCharCode(65 + (i % 26)));
    return chars.join('');
}

let size = 10000;
let rope = makeDeepRope(size);
let flat = makeFlat(size);

// Correctness: slice results must match flat string
for (let i = 0; i < size; i += 100) {
    let len = Math.min(5, size - i);
    let ropeSlice = rope.slice(i, i + len);
    let flatSlice = flat.slice(i, i + len);
    if (ropeSlice !== flatSlice)
        throw new Error(`Mismatch at ${i}: '${ropeSlice}' vs '${flatSlice}'`);
}

// Cross-fiber slices (various lengths spanning potential fiber boundaries)
for (let len = 1; len <= 10; len++) {
    for (let i = 0; i < size - len; i += 97) {
        let ropeSlice = rope.slice(i, i + len);
        let flatSlice = flat.slice(i, i + len);
        if (ropeSlice !== flatSlice)
            throw new Error(`Cross-fiber mismatch at ${i}, len=${len}: '${ropeSlice}' vs '${flatSlice}'`);
    }
}

// Edge cases
if (rope.slice(0, 0) !== '')
    throw new Error('Empty slice failed');
if (rope.slice(0, size) !== flat)
    throw new Error('Full slice failed');
if (rope.slice(size - 1, size) !== flat.slice(size - 1, size))
    throw new Error('Last char slice failed');
