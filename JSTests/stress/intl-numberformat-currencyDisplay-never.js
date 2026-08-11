//@ requireOptions("--useMoreCurrencyDisplayChoices=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

{
    const nf = new Intl.NumberFormat("zh-TW", {
        style: "currency",
        currency: "TWD",
        currencyDisplay: "never",
    });
    shouldBe(nf.format(42), "42.00");
}

{
    const nf = new Intl.NumberFormat("en-US", {
        style: "currency",
        currency: "USD",
        currencyDisplay: "never",
    });
    shouldBe(nf.format(42), "42.00");
}

{
    const nf = new Intl.NumberFormat("ja-JP", {
        style: "currency",
        currency: "JPY",
        currencyDisplay: "never",
    });
    shouldBe(nf.format(42), "42");
}

