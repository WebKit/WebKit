
function test(a, b) {
    const left = a % b;
    const right = a - b;

    if (a >- 0n) {
        a = left;     
    } else {
        b = right;
    }
    return a + b;
}

for (let i = 1e4; i >= 0n; i--) {
    test(1n, 1n);
}
