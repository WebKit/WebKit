function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

if ($vm.icuVersion() >= 64) {
    shouldBe((299792458).toLocaleString("ja-JP", {
        style: "unit",
        unit: "meter-per-second",
        unitDisplay: "short"
    }), `299,792,458 m/s`);

    shouldBe((987654321).toLocaleString("ja-JP", {
        notation: "scientific"
    }), `9.877E8`);

    shouldBe((987654321).toLocaleString("ja-JP", {
        notation: "engineering"
    }), `987.654E6`);

    shouldBe((987654321).toLocaleString("ja-JP", {
        notation: "compact",
        compactDisplay: "long"
    }), `9.9億`);

    shouldBe((299792458).toLocaleString("ja-JP", {
        notation: "scientific",
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
        style: "unit",
        unit: "meter-per-second"
    }), `3.00E8 m/s`);

    shouldBe((55).toLocaleString("ja-JP", {
        signDisplay: "always"
    }), `+55`);

    shouldBe((-100).toLocaleString("ja-JP", {
        style: "currency",
        currency: "EUR",
        currencySign: "accounting"
    }), `(€100.00)`);

    shouldBe((0.55).toLocaleString("ja-JP", {
        style: "percent",
        signDisplay: "exceptZero"
    }), `+55%`);

    shouldBe((100).toLocaleString("ja-JP", {
        style: "currency",
        currency: "JPY",
        currencyDisplay: "narrowSymbol"
    }), `￥100`);
}
