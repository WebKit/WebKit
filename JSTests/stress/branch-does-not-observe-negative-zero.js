function shouldBe(actual, expected, what) {
    if (!Object.is(actual, expected))
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

function ifMod(x, y) { if (x % y) return 1; return 0; }
noInline(ifMod);

function ifNotMod(x, y) { if (!(x % y)) return 1; return 0; }
noInline(ifNotMod);

function ifInt52Mod(x, y) { if ((x * 4294967296) % y) return 1; return 0; }
noInline(ifInt52Mod);

function ifMul(x, y) { return (x * y) ? 1 : 0; }
noInline(ifMul);

function ifDiv(x, y) { if (x / y) return 1; return 0; }
noInline(ifDiv);

function ifNegate(x) { if (-x) return 1; return 0; }
noInline(ifNegate);

function ifAdd(x, y) { if (x + y) return 1; return 0; }
noInline(ifAdd);

function modAfterBranch(x, y) { var r = x % y; if (r) return 1; return r; }
noInline(modAfterBranch);

function mulAfterBranch(x, y) { var r = x * y; if (!r) return r; return 1; }
noInline(mulAfterBranch);

function modAcrossBlocks(x, y, z) { var r = z ? x % y : x; if (r) return 1; return r; }
noInline(modAcrossBlocks);

for (let i = 1; i < testLoopCount; ++i) {
    shouldBe(ifMod(-i, i), 0, "if (-i % i)");
    shouldBe(ifMod(-i, i + 1), 1, "if (-i % (i + 1))");
    shouldBe(ifNotMod(-i, i), 1, "if (!(-i % i))");
    shouldBe(ifNotMod(-i, i + 1), 0, "if (!(-i % (i + 1)))");
    shouldBe(ifInt52Mod(-i, i), 0, "if ((-i * 2^32) % i)");
    shouldBe(ifInt52Mod(-3, 4294967297), 1, "if ((-3 * 2^32) % (2^32 + 1))");
    shouldBe(ifMul(i, 0), 0, "if (i * 0)");
    shouldBe(ifMul(-i, 1), 1, "if (-i * 1)");
    shouldBe(ifDiv(0, i), 0, "if (0 / i)");
    shouldBe(ifDiv(-i, i), 1, "if (-i / i)");
    shouldBe(ifNegate(i), 1, "if (-i)");
    shouldBe(ifAdd(-i, i), 0, "if (-i + i)");
    shouldBe(ifAdd(i, i), 1, "if (i + i)");
    shouldBe(modAfterBranch(i, i), 0, "i % i after a branch");
    shouldBe(modAfterBranch(-i, i), -0, "-i % i after a branch");
    shouldBe(mulAfterBranch(i, 0), 0, "i * 0 after a branch");
    shouldBe(mulAfterBranch(-i, 1), 1, "-i * 1 after a branch");
    shouldBe(modAcrossBlocks(i, i, true), 0, "i % i across blocks");
    shouldBe(modAcrossBlocks(-i, i, true), -0, "-i % i across blocks");
}

shouldBe(ifMul(-5, 0), 0, "if (-5 * 0)");
shouldBe(ifDiv(0, -5), 0, "if (0 / -5)");
shouldBe(ifNegate(0), 0, "if (-0)");
shouldBe(mulAfterBranch(-5, 0), -0, "-5 * 0 after a branch");
shouldBe(ifMod(5, 0), 0, "if (5 % 0)");
shouldBe(ifNotMod(5, 0), 1, "if (!(5 % 0))");
shouldBe(ifInt52Mod(5, 0), 0, "if ((5 * 2^32) % 0)");
shouldBe(ifMod(-2147483648, -1), 0, "if (-2^31 % -1)");
shouldBe(ifMod(0.5, 1), 1, "if (0.5 % 1)");
shouldBe(ifMul(65536, 65536), 1, "if (2^16 * 2^16)");
shouldBe(ifMul(Infinity, 0), 0, "if (Infinity * 0)");
shouldBe(ifDiv(1, 2), 1, "if (1 / 2)");
shouldBe(ifDiv(1, 0), 1, "if (1 / 0)");
shouldBe(ifDiv(0, 0), 0, "if (0 / 0)");
shouldBe(ifNegate(-2147483648), 1, "if (-(-2^31))");
shouldBe(ifNegate(NaN), 0, "if (-NaN)");
shouldBe(ifAdd(-2147483648, -2147483648), 1, "if (-2^31 + -2^31)");
shouldBe(ifAdd(Infinity, -Infinity), 0, "if (Infinity + -Infinity)");
