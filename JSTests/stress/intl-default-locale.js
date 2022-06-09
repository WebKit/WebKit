function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// Actual default should always be a string
shouldBe(typeof new Intl.DateTimeFormat().resolvedOptions().locale, 'string');
shouldBe(typeof new Intl.NumberFormat().resolvedOptions().locale, 'string');
shouldBe(typeof new Intl.Collator().resolvedOptions().locale, 'string');

// Actual default should always be canonicalized
shouldBe(Intl.getCanonicalLocales(new Intl.DateTimeFormat().resolvedOptions().locale)[0], new Intl.DateTimeFormat().resolvedOptions().locale);
shouldBe(Intl.getCanonicalLocales(new Intl.NumberFormat().resolvedOptions().locale)[0], new Intl.NumberFormat().resolvedOptions().locale);
shouldBe(Intl.getCanonicalLocales(new Intl.Collator().resolvedOptions().locale)[0], new Intl.NumberFormat().resolvedOptions().locale);

// Any language name with less than two characters is considered invalid, so we use "a" here.
// "i-klingon" is grandfathered, and is canonicalized "tlh".
// It should not be part of any available locale sets, so we know it came from here.
$vm.setUserPreferredLanguages([ "a", "*", "en_US.utf8", "i-klingon", "en-US" ]);
shouldBe(new Intl.DateTimeFormat().resolvedOptions().locale, 'tlh');
shouldBe(new Intl.NumberFormat().resolvedOptions().locale, 'tlh');
shouldBe(new Intl.Collator().resolvedOptions().locale, 'tlh');
