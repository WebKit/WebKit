const tag = 'ja-Hant-KR-u-ca-buddhist-co-eor-hc-h11-kf-lower-kn-false-nu-thai';
const locale = new Intl.Locale(tag);

function shouldBeSameWithTagOrLocale(func) {
    const actual = func(locale);
    const expected = func(tag);
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBeSameWithTagOrLocale(tagOrLocale => tagOrLocale.toString());
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.Locale(tagOrLocale).toString());

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.getCanonicalLocales(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.getCanonicalLocales(['en', tagOrLocale, 'fr'])));

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.Collator.supportedLocalesOf(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.Collator.supportedLocalesOf(['en', tagOrLocale, 'fr'])));
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.Collator(tagOrLocale).resolvedOptions().locale);

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf(['en', tagOrLocale, 'fr'])));
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.DateTimeFormat(tagOrLocale).resolvedOptions().locale);

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.NumberFormat.supportedLocalesOf(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.NumberFormat.supportedLocalesOf(['en', tagOrLocale, 'fr'])));
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.NumberFormat(tagOrLocale).resolvedOptions().locale);

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.PluralRules.supportedLocalesOf(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.PluralRules.supportedLocalesOf(['en', tagOrLocale, 'fr'])));
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.PluralRules(tagOrLocale).resolvedOptions().locale);

shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf(tagOrLocale)));
shouldBeSameWithTagOrLocale(tagOrLocale => JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf(['en', tagOrLocale, 'fr'])));
shouldBeSameWithTagOrLocale(tagOrLocale => new Intl.RelativeTimeFormat(tagOrLocale).resolvedOptions().locale);

shouldBeSameWithTagOrLocale(tagOrLocale => [3].toLocaleString(tagOrLocale));
shouldBeSameWithTagOrLocale(tagOrLocale => 3n.toLocaleString(tagOrLocale));
shouldBeSameWithTagOrLocale(tagOrLocale => new Date(0).toLocaleString(tagOrLocale, { timeZone: 'UTC' }));
shouldBeSameWithTagOrLocale(tagOrLocale => 3..toLocaleString(tagOrLocale));
shouldBeSameWithTagOrLocale(tagOrLocale => 'ä'.localeCompare('a', tagOrLocale));
shouldBeSameWithTagOrLocale(tagOrLocale => 'Æ'.toLocaleLowerCase(tagOrLocale));
shouldBeSameWithTagOrLocale(tagOrLocale => 'æ'.toLocaleUpperCase(tagOrLocale));
