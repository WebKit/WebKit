//@ runDefault("--useDFGJIT=false")

function assert(a, message) {
    if (!a)
        throw new Error(message);
}

let a = (1n << 1048575n) - 1n;
a = (a << 1n) | 1n;

try {
    let b = a + 1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a - (-1n);
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a * (-1n);
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a / a;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = -a & -1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

try {
    let b = a ^ -1n;
    assert(false, "Should throw OutOfMemoryError, but executed without exception");
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big", "Expected OutOfMemoryError, but got: " + e);
}

