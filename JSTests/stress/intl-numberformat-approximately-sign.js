function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}

var fmt = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "USD",
  maximumFractionDigits: 0,
});

if (fmt.formatRange && fmt.formatRangeToParts) {
    if ($vm.icuVersion() < 71) {
        shouldBe(fmt.formatRange(42, 42), `~$42`);
        shouldBe(fmt.formatRange(41.999, 42.195), `~$42`);
        shouldBe(JSON.stringify(fmt.formatRangeToParts(42, 42)), `[{"type":"currency","value":"$","source":"shared"},{"type":"integer","value":"42","source":"shared"}]`);
        shouldBe(JSON.stringify(fmt.formatRangeToParts(41.999, 42.195)), `[{"type":"currency","value":"$","source":"shared"},{"type":"integer","value":"42","source":"shared"}]`);
    } else {
        shouldBe(fmt.formatRange(42, 42), `~$42`);
        shouldBe(fmt.formatRange(41.999, 42.195), `~$42`);
        shouldBe(JSON.stringify(fmt.formatRangeToParts(42, 42)), `[{"type":"approximatelySign","value":"~","source":"shared"},{"type":"currency","value":"$","source":"shared"},{"type":"integer","value":"42","source":"shared"}]`);
        shouldBe(JSON.stringify(fmt.formatRangeToParts(41.999, 42.195)), `[{"type":"approximatelySign","value":"~","source":"shared"},{"type":"currency","value":"$","source":"shared"},{"type":"integer","value":"42","source":"shared"}]`);
    }
}
