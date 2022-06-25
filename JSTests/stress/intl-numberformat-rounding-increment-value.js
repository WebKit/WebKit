function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}
shouldBe(new Intl.NumberFormat("en", { minimumFractionDigits: 3, maximumFractionDigits: 3, roundingIncrement: 10 }).format(55555555555.555555), `55,555,555,555.560`);
shouldBe(new Intl.NumberFormat("en", { minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 100 }).format(55555555555.555555), `55,555,555,556.00`);
shouldBe(new Intl.NumberFormat("en", { minimumFractionDigits: 2, maximumFractionDigits: 4, roundingIncrement: 1000 }).format(55555555555.555555), `55,555,555,555.6000`);
shouldBe(new Intl.NumberFormat("en", { minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 1000 }).format(55555555555.555555), `55,555,555,560.00`);
shouldBe(new Intl.NumberFormat("en", { minimumFractionDigits: 1, maximumFractionDigits: 1, roundingIncrement: 1000 }).format(55555555555.555555), `55,555,555,600.0`);
