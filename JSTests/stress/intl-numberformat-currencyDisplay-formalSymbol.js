//@ requireOptions("--useMoreCurrencyDisplayChoices=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}


// TODO: We need to add more tests but but CLDR is not for ready
// https://github.com/tc39/proposal-intl-currency-display-choices/issues/3
{
    const nf = new Intl.NumberFormat("zh-TW", {
        style: "currency",
        currency: "TWD",
        currencyDisplay: "formalSymbol",
    });
    shouldBe(nf.format(42), "NT$42.00");
}

