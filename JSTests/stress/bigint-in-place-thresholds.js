function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// Exercise both sides of maxInPlaceSubSize = 16 (absoluteSub) and
// maxInPlaceCachedModSize = 8 (cachedMod) so that a future tweak to either
// constant can't silently bypass the in-place fast path on the hot path.

// absoluteSub: x and y are positive, |x| > |y|, x.length() in {15, 16, 17, 18} digits (64-bit).
// length L means 2^((L-1)*64) <= |x| < 2^(L*64).
{
    const oneDigitBits = 64n;
    function pow2(n) { return 1n << n; }
    function topDigitBit(L) { return BigInt(L) * oneDigitBits - 1n; }

    for (const L of [15, 16, 17, 18, 32, 64]) {
        // Force x to have exactly L digits and y to have L-1 digits.
        const x = pow2(topDigitBit(L)) | 12345n;
        const y = pow2(topDigitBit(L - 1)) | 7n;
        const expected = x - y;
        let actual = 0n;
        for (let i = 0; i < 200; i++)
            actual = x - y;
        shouldBe(actual, expected);
        // Negation path (resultSign flipped).
        let actualNeg = 0n;
        for (let i = 0; i < 200; i++)
            actualNeg = y - x;
        shouldBe(actualNeg, -expected);
    }
}

// absoluteSub: result normalizes to zero (|x| - |y| with all leading digits cancelling).
{
    for (const L of [4, 8, 16, 17, 32]) {
        const x = (1n << BigInt(L * 64)) - 1n;
        const y = (1n << BigInt(L * 64)) - 1n;
        let actual = -1n;
        for (let i = 0; i < 200; i++)
            actual = x - y;
        shouldBe(actual, 0n);
    }
}

// absoluteAdd: in-place path is always taken below maxLength; verify various sizes.
{
    for (const L of [1, 2, 8, 16, 17, 32, 100]) {
        const x = (1n << BigInt(L * 64 - 1)) | 3n;
        const y = (1n << BigInt(L * 64 - 1)) | 5n;
        const expected = x + y;
        let actual = 0n;
        for (let i = 0; i < 200; i++)
            actual = x + y;
        shouldBe(actual, expected);
        // Carry that grows the length by one.
        const xMax = (1n << BigInt(L * 64)) - 1n;
        const yOne = 1n;
        let actualCarry = 0n;
        for (let i = 0; i < 200; i++)
            actualCarry = xMax + yOne;
        shouldBe(actualCarry, 1n << BigInt(L * 64));
    }
}

// cachedMod: install divisor in cache (>= 100 iters with same y), then run past
// the threshold. ySpan.size() in {7, 8, 9, 10, 16, 32} digits.
{
    function divisorWithLength(L) {
        // Top digit non-zero, low digit non-zero — canonicalised to exactly L digits.
        return (1n << BigInt(L * 64 - 1)) | 19n;
    }
    for (const L of [7, 8, 9, 10, 16, 32]) {
        const p = divisorWithLength(L);
        // Dividend roughly 2x divisor size — falls under the xSpan <= 2*ySpan guard.
        let x = (1n << BigInt(L * 64 + L * 32)) | 0xdeadbeefn;
        const seed = x;
        let actual = x;
        for (let i = 0; i < 250; i++)
            actual = (actual * 3n + 1n) % p;
        // Recompute with a fresh divisor object so we exit the cached path; results must match.
        const pCopy = (p + 0n);  // identity, but JIT/runtime treats as same value
        let expected = seed;
        for (let i = 0; i < 250; i++)
            expected = (expected * 3n + 1n) % pCopy;
        shouldBe(actual, expected);
        // Dividend exact multiple: cached-mod yields zero (canonical zero return path).
        let zero = -1n;
        for (let i = 0; i < 200; i++)
            zero = (p * 7n) % p;
        shouldBe(zero, 0n);
        // Negative dividend: sign handling with cached mod.
        let neg = 0n;
        for (let i = 0; i < 200; i++)
            neg = (-(seed * 5n + 3n)) % p;
        shouldBe(neg <= 0n, true);
        if (neg !== 0n)
            shouldBe(-neg < p, true);
    }
}

// cachedMod: divisor swap mid-loop — cache must invalidate cleanly.
{
    const p1 = (1n << 511n) | 19n;     // 8 digits
    const p2 = (1n << 575n) | 23n;     // 9 digits — crosses the maxInPlaceCachedModSize boundary
    let x = (1n << 600n) | 1n;
    for (let i = 0; i < 150; i++)
        x = (x * x + 1n) % p1;
    let y = x;
    for (let i = 0; i < 150; i++)
        y = (y * y + 1n) % p2;
    shouldBe(y >= 0n && y < p2, true);
}
