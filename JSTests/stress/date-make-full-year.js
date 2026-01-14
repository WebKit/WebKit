function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected}, but got ${actual}`);
}

shouldBe(new Date(0, 0).getFullYear(), 1900);
shouldBe(new Date(1, 0).getFullYear(), 1901);
shouldBe(new Date(50, 0).getFullYear(), 1950);
shouldBe(new Date(98, 0).getFullYear(), 1998);
shouldBe(new Date(99, 0).getFullYear(), 1999);

shouldBe(new Date(100, 0).getFullYear(), 100);
shouldBe(new Date(1900, 0).getFullYear(), 1900);
shouldBe(new Date(2024, 0).getFullYear(), 2024);
shouldBe(new Date(9999, 0).getFullYear(), 9999);

shouldBe(new Date(-1, 0).getFullYear(), -1);
shouldBe(new Date(-100, 0).getFullYear(), -100);

// Fractional years truncate
shouldBe(new Date(0.9, 0).getFullYear(), 1900);
shouldBe(new Date(50.1, 0).getFullYear(), 1950);
shouldBe(new Date(50.9, 0).getFullYear(), 1950);
shouldBe(new Date(99.9, 0).getFullYear(), 1999);
shouldBe(new Date(100.1, 0).getFullYear(), 100);
shouldBe(new Date(100.9, 0).getFullYear(), 100);

shouldBe(new Date(Date.UTC(0, 0)).getUTCFullYear(), 1900);
shouldBe(new Date(Date.UTC(50, 0)).getUTCFullYear(), 1950);
shouldBe(new Date(Date.UTC(99, 0)).getUTCFullYear(), 1999);
shouldBe(new Date(Date.UTC(100, 0)).getUTCFullYear(), 100);
shouldBe(new Date(Date.UTC(2024, 0)).getUTCFullYear(), 2024);

// NaN
shouldBe(isNaN(new Date(NaN, 0).getTime()), true);
