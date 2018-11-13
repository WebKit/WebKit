//@ runDefault("--useBigInt=true", "--useDFGJIT=false")

function assert(a, message) {
    if (!a)
        throw new Error(message);
}

function lshift(y) {
    let out = 1n;
    for (let i = 0; i < y; i++) {
        out *= 2n;
    }

    return out;
}

let a = lshift(16384 * 63);
for (let i = 0; i < 256; i++) {
    a *= 18446744073709551615n;
}

try {
    let b = a + 1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a - (-1n);
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a * (-1n);
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a / a;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = -a & -1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a ^ -1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory", "Expected OutOfMemoryError, but got: " + e);
}

