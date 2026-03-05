function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var fmt = new Intl.NumberFormat('en-US', {style: 'currency', currency: 'USD', notation: 'compact', maximumFractionDigits: 2, compactDisplay: 'long'});

shouldBe(JSON.stringify(fmt.resolvedOptions()), `{"locale":"en-US","numberingSystem":"latn","style":"currency","currency":"USD","currencyDisplay":"symbol","currencySign":"standard","minimumIntegerDigits":1,"minimumFractionDigits":0,"maximumFractionDigits":2,"useGrouping":"min2","notation":"compact","compactDisplay":"long","signDisplay":"auto","roundingIncrement":1,"roundingMode":"halfExpand","roundingPriority":"auto","trailingZeroDisplay":"auto"}`);
shouldBe(fmt.format(97896), `$97.9K`);

var fmt = new Intl.NumberFormat('en-US', {style: 'decimal', notation: 'compact', maximumFractionDigits: 2, compactDisplay: 'long'})
shouldBe(JSON.stringify(fmt.resolvedOptions()), `{"locale":"en-US","numberingSystem":"latn","style":"decimal","minimumIntegerDigits":1,"minimumFractionDigits":0,"maximumFractionDigits":2,"useGrouping":"min2","notation":"compact","compactDisplay":"long","signDisplay":"auto","roundingIncrement":1,"roundingMode":"halfExpand","roundingPriority":"auto","trailingZeroDisplay":"auto"}`);
shouldBe(fmt.format(97896), `97.9 thousand`);
