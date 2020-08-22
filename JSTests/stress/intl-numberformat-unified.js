function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

if ($vm.icuVersion() >= 64) {
    shouldBe((299792458).toLocaleString("en-US", {
        style: "unit",
        unit: "meter-per-second",
        unitDisplay: "short"
    }), `299,792,458 m/s`);

    shouldBe((987654321).toLocaleString("en-US", {
        notation: "scientific"
    }), `9.877E8`);

    shouldBe((987654321).toLocaleString("en-US", {
        notation: "engineering"
    }), `987.654E6`);

    shouldBe((987654321).toLocaleString("en-US", {
        notation: "compact",
        compactDisplay: "long"
    }), `988 million`);

    shouldBe((299792458).toLocaleString("en-US", {
        notation: "scientific",
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
        style: "unit",
        unit: "meter-per-second"
    }), `3.00E8 m/s`);

    shouldBe((55).toLocaleString("en-US", {
        signDisplay: "always"
    }), `+55`);

    shouldBe((-100).toLocaleString("bn", {
        style: "currency",
        currency: "EUR",
        currencySign: "accounting"
    }), `(১০০.০০€)`);

    shouldBe((0.55).toLocaleString("en-US", {
        style: "percent",
        signDisplay: "exceptZero"
    }), `+55%`);

    shouldBe((100).toLocaleString("en-CA", {
        style: "currency",
        currency: "USD",
        currencyDisplay: "narrowSymbol"
    }), `$100.00`);
}
