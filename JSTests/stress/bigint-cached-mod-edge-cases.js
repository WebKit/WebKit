function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// cachedMod is activated after 100 uses of the same divisor.
// It requires: divisor has 2-32 64-bit digits, dividend has n to 2n digits.
// multiplySpecialHigh computes only digits from startPos = 2n-2 upward.
// All expected values verified against V8 (Node.js).

// --- n=2 (128-bit divisor, minimum for cachedMod) ---
// startPos = 2*2-2 = 2, tightest interaction between highSpan and productLow.

// n=2: squaring produces 2n-digit dividend (maximum for cached path).
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 200n) + 7n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 177278552475415211552762206261272111601n);
}

// n=2: multiply-by-small produces n+1 digit dividend.
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 127n) + 1n;
    for (let i = 0; i < 200; i++)
        x = (x * 3n) % p;
    shouldBe(x, 270852476855807815921471953649862209043n);
}

// n=2: multiply-by-(p-1) produces exactly 2n digit dividend every iteration.
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 127n) + 1n;
    for (let i = 0; i < 200; i++)
        x = (x * (p - 1n)) % p;
    shouldBe(x, 170141183460469231731687303715884105729n);
}

// n=2: dividend < divisor (same n digits, |x| < |y| returns early before cachedMod).
{
    const p = (1n << 128n) - 159n;
    let x = p - 1n;
    for (let i = 0; i < 150; i++)
        x = x % p;
    shouldBe(x, p - 1n);
}

// n=2: exact multiple gives 0.
{
    const p = (1n << 128n) - 159n;
    let x = p * 3n;
    for (let i = 0; i < 150; i++)
        x = x % p;
    shouldBe(x, 0n);
}

// n=2: dividend = divisor + 1.
{
    const p = (1n << 128n) - 159n;
    let x = p + 1n;
    for (let i = 0; i < 150; i++)
        x = x % p;
    shouldBe(x, 1n);
}

// n=2: all-ones divisor (maximize internal carries in multiply).
{
    const p = (1n << 128n) - 1n;
    let x = (1n << 255n) + 123456789n;
    for (let i = 0; i < 200; i++)
        x = (x * 3n) % p;
    shouldBe(x, 78817515762451186854519328032592602042n);
}

// n=2: Mersenne-like divisor, squaring.
{
    const p = (1n << 128n) - 1n;
    let x = (1n << 255n) - 1n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 1n);
}

// n=2: divisor 2^64 + 1 (smallest 2-digit divisor).
{
    const p = (1n << 64n) + 1n;
    let x = (1n << 127n) + 1n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 4831328171988212771n);
}

// n=2: divisor 2^127 + 1 (close to digit boundary).
{
    const p = (1n << 127n) + 1n;
    let x = (1n << 200n) + 99n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 150824840280771579839960814384354169801n);
}

// n=2: carry-stress divisor pattern.
{
    const p = (1n << 127n) + (1n << 64n) - 1n;
    let x = (1n << 200n) + 12345n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 11609820357105566628172548015849534484n);
}

// n=2: forced exactly-2n digit dividend every iteration.
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 200n) + 1n;
    for (let i = 0; i < 200; i++) {
        x = x % p;
        x = x * (p >> 1n);
    }
    shouldBe(x % p, 314748738897955976014696882727821759065n);
}

// n=2: >2n digit dividend bypasses cachedMod (falls to textbook).
// Verify correctness at the boundary between cached and non-cached paths.
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 320n) + 1n;
    for (let i = 0; i < 200; i++) {
        x = x % p;
        x = x * (1n << 250n) + 1n;
    }
    shouldBe(x % p, 291926710485922420603726882946018966276n);
}

// n=2: alternating between cached (<=2n digits) and non-cached (>2n digits) dividend sizes.
{
    const p = (1n << 128n) - 159n;
    let x = (1n << 200n) + 1n;
    for (let i = 0; i < 200; i++) {
        if (i % 3 === 0)
            x = (x * x) % p;
        else if (i % 3 === 1)
            x = (x * 3n) % p;
        else {
            x = x % p;
            x = x * (1n << 250n) + BigInt(i);
        }
    }
    shouldBe(x % p, 115432987156021975549902458353369394892n);
}

// --- n=3 (192-bit divisor, startPos = 4) ---

// n=3: squaring produces 2n-digit dividend.
{
    const p = (1n << 192n) - 237n;
    let x = (1n << 300n) + 42n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 4783151374147123437769052551498427100842950442371165078084n);
}

// n=3: multiply-by-small produces n+1 digit dividend.
{
    const p = (1n << 192n) - 237n;
    let x = (1n << 191n) + 1n;
    for (let i = 0; i < 200; i++)
        x = (x * 5n) % p;
    shouldBe(x, 5416697097689928077577136525344415119217528154785858185433n);
}

// n=3: same-size dividend stays unchanged.
{
    const p = (1n << 192n) - 237n;
    let x = p - 7n;
    for (let i = 0; i < 150; i++)
        x = x % p;
    shouldBe(x, p - 7n);
}

// --- n=4 (256-bit divisor, common crypto size, startPos = 6) ---

// n=4: multiply-by-constant produces n+1 digit dividend.
{
    const p = 2n ** 255n - 19n;
    let x = 2n ** 510n - 1n;
    for (let i = 0; i < 200; i++)
        x = (x * 7n) % p;
    shouldBe(x, 20199529111670185067226509400580693219090559440950920314010470048972233890619n);
}

// n=4: multiply-by-small produces n+1 digit dividend.
{
    const p = 2n ** 255n - 19n;
    let x = (1n << 280n) + 999n;
    for (let i = 0; i < 200; i++)
        x = (x * 3n) % p;
    shouldBe(x, 15996362973740392162448859893859295679701168986134187252638248410264421490236n);
}

// n=4: mixed dividend sizes within the same loop.
{
    const p = 2n ** 255n - 19n;
    let x = (1n << 400n) + 1n;
    for (let i = 0; i < 200; i++) {
        x = x % p;
        if (i % 2 === 0)
            x = x * x;
        else
            x = x * 5n;
    }
    shouldBe(x % p, 27989761200312039329451992067769596918851024311859999205911201460129320658795n);
}

// n=4: dividend exactly 2n digits (maximum for cached path).
{
    const p = 2n ** 255n - 19n;
    let x = 2n ** 510n - 1n;
    for (let i = 0; i < 150; i++) {
        x = x % p;
        x = x * (1n << 254n) + 1n;
    }
    shouldBe(x >= 0n, true);
    shouldBe(x % p, 24510358332628122011763128125706199547846894726910051373319518975690675744707n);
}

// --- n=8 (512-bit divisor, startPos = 14) ---

{
    const p = (1n << 512n) - 38117n;
    let x = (1n << 1000n) + 1n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 3259050154561516332303492723294531008767950455989810480963402987785461964981764930977751014809157529428079184366202236463921664022928513497208112346500620n);
}

// --- n=16 (1024-bit divisor, startPos = 30) ---

{
    const p = (1n << 1024n) - 105n;
    let x = (1n << 2000n) + 999n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 178640561875409494929959031951624405717939340969517783329258186957487830890452848788419238713797232807467856250187421907180187905237092645402923409446182195299760241272859355506076099790889384520936258763333705405745006328840429412195813753120246479007446433879238586201467087653339820638869436278379447077794n);
}

// --- n=32 (2048-bit divisor, maximum allowed, startPos = 62) ---

{
    const p = (1n << 2048n) - 1189n;
    let x = (1n << 4000n) + 77n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 12501303954234731296448305243218988457206472929459428325391125443525616770649464934923442478929335110320775381860379284470818061433235087642149480108602109741325437864117022440644834656017346537017296922710715260139268429585823463895889358743920640589646953755200915797622448646367060771335000636804296992123842316933423259427755033853591118477421542532328149250860266938827537500361258798796555009465639219810429378537679375462752876788798667730576909971366882045210212101958659351598990798365193220298815896698704445967436601475994366721210115982085568357494806430993248085312179545300746077781068475543829440968113n);
}

// --- Negative dividends ---

{
    const p2 = (1n << 128n) - 159n;
    let x2 = -(1n << 200n);
    for (let i = 0; i < 150; i++)
        x2 = x2 % p2;
    shouldBe(x2 <= 0n, true);
    shouldBe(-x2 < p2, true);

    const p4 = 2n ** 255n - 19n;
    let x4 = -(1n << 500n);
    for (let i = 0; i < 150; i++)
        x4 = x4 % p4;
    shouldBe(x4 <= 0n, true);
    shouldBe(-x4 < p4, true);
}

// --- Divisor size boundaries ---

// Dividend = 1 stays 1 for any divisor > 1.
{
    const primes = [
        (1n << 128n) - 159n,
        (1n << 192n) - 237n,
        2n ** 255n - 19n,
        (1n << 512n) - 38117n,
    ];
    for (const p of primes) {
        let x = 1n;
        for (let i = 0; i < 150; i++)
            x = x % p;
        shouldBe(x, 1n);
    }
}

// Dividend = divisor - 1 stays unchanged for all sizes.
{
    const sizes = [128n, 192n, 256n, 512n];
    for (const bits of sizes) {
        const p = (1n << bits) - 1n;
        let x = p - 1n;
        for (let i = 0; i < 150; i++)
            x = x % p;
        shouldBe(x, p - 1n);
    }
}

// --- Cache invalidation ---

// Switch divisors at same n=2 size.
{
    const p1 = (1n << 128n) - 159n;
    const p2 = (1n << 128n) - 173n;
    let x = (1n << 200n) + 1n;
    for (let i = 0; i < 150; i++)
        x = (x * x) % p1;
    for (let i = 0; i < 150; i++)
        x = (x * x) % p2;
    shouldBe(x >= 0n && x < p2, true);
}

// Switch divisors across sizes: n=2 then n=4.
{
    const p1 = (1n << 128n) - 159n;
    const p2 = 2n ** 255n - 19n;
    let x = (1n << 200n) + 1n;
    for (let i = 0; i < 150; i++)
        x = (x * x) % p1;
    for (let i = 0; i < 150; i++)
        x = (x * x) % p2;
    shouldBe(x >= 0n && x < p2, true);
}

// --- Division identity: x === (x / p) * p + (x % p) ---

{
    const p = (1n << 256n) - 189n;
    let dummy = 0n;
    for (let i = 0; i < 150; i++)
        dummy = ((1n << 300n) + BigInt(i)) % p;

    for (let i = 0; i < 50; i++) {
        let x = (1n << (256n + BigInt(i))) + BigInt(i * 997);
        let result = x % p;
        shouldBe(result >= 0n && result < p, true);
        shouldBe(x - result === (x / p) * p, true);
    }
}

// --- Exact multiples ---

{
    const p = (1n << 128n) - 159n;
    for (let i = 0; i < 150; i++) {
        let x = p * BigInt(i + 1);
        shouldBe(x % p, 0n);
    }
}

// --- Consecutive values near divisor boundary ---

{
    const p = (1n << 128n) - 159n;
    for (let i = 0; i < 150; i++) {
        let x = p * 2n + BigInt(i);
        shouldBe(x % p, BigInt(i));
    }
}
