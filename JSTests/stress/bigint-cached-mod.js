function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// Test 1: Repeated modulo with same divisor (triggers caching after 100 iterations).
{
    const p = 2n ** 255n - 19n; // ed25519 prime
    let x = 12345678901234567890123456789n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 36260996769751896565040950513605801167497797310772705034288993169759633740077n);
}

// Test 2: Various dividend sizes relative to divisor.
{
    const divisor = (1n << 256n) - 189n;
    let results = [];
    // Dividend close to divisor size (n digits)
    let a = (1n << 255n) + 17n;
    for (let i = 0; i < 150; i++) {
        a = (a * 3n) % divisor;
    }
    results.push(a);

    // Dividend close to 2*divisor size (2n digits)
    let b = (1n << 500n) + 42n;
    for (let i = 0; i < 150; i++) {
        b = (b % divisor);
        b = b * (1n << 250n) + 1n;
    }
    results.push(b % divisor);

    // Verify all results are in range [0, divisor)
    for (let r of results) {
        if (r < 0n || r >= divisor)
            throw new Error(`Result ${r} out of range`);
    }
}

// Test 3: Divisor changes mid-loop (cache should invalidate).
{
    const p1 = 2n ** 255n - 19n;
    const p2 = 2n ** 256n - 189n;
    let x = 999999999999999999999999999999n;
    for (let i = 0; i < 120; i++)
        x = (x * x) % p1;
    let y = x;
    for (let i = 0; i < 120; i++)
        y = (y * y) % p2;
    // Just make sure it doesn't crash and produces a reasonable result.
    shouldBe(y >= 0n && y < p2, true);
}

// Test 4: Small divisors that should NOT be cached (single-digit fast path).
{
    let x = 2n ** 300n;
    for (let i = 0; i < 200; i++)
        x = x % 7n;
    shouldBe(x, (2n ** 300n) % 7n);
}

// Test 5: Sign handling with cached mod.
{
    const p = 2n ** 255n - 19n;
    let x = -(2n ** 300n + 1n);
    for (let i = 0; i < 150; i++)
        x = x % p;
    // Negative dividend: result should be negative or zero.
    shouldBe(x <= 0n, true);
    shouldBe(-x < p, true);
}

// Test 6: Exact multiple (result should be 0).
{
    const p = 2n ** 255n - 19n;
    for (let i = 0; i < 150; i++) {
        let x = p * BigInt(i + 1);
        shouldBe(x % p, 0n);
    }
}
