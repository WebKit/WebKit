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

$vm.setUserPreferredLanguages([ "fr-FR" ]);
// Intl.DateTimeFormat caches its resolved formatter and only revalidates it on VM entry, so a
// language change takes effect on the next task turn rather than synchronously within this one.
setTimeout(() => {
    shouldBe(new Intl.DateTimeFormat().resolvedOptions().locale, 'fr-FR');
    shouldBe(new Intl.NumberFormat().resolvedOptions().locale, 'fr-FR');
    shouldBe(new Intl.Collator().resolvedOptions().locale, 'fr-FR');
}, 0);
