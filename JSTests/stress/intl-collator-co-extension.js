function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de", {usage: "sort"}).compare), ["\u00C4", "AE"]);
shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de", {usage: "search"}).compare), ["AE", "\u00C4"]);
shouldBe(new Intl.Collator("de", {usage: "sort"}).resolvedOptions().locale, "de");
shouldBe(new Intl.Collator("de", {usage: "search"}).resolvedOptions().locale, "de");
shouldBe(new Intl.Collator("de", {usage: "sort", collation: "phonebk"}).resolvedOptions().locale, "de");
shouldBeArray(["2", "10"].sort(new Intl.Collator("de", {usage: "sort"}).compare), ["10", "2"]);
shouldBeArray(["2", "10"].sort(new Intl.Collator("de", {usage: "search"}).compare), ["10", "2"]);

shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de-u-co-search", {usage: "sort"}).compare), ["\u00C4", "AE"]);
shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de-u-co-sort", {usage: "search"}).compare), ["AE", "\u00C4"]);
shouldBe(new Intl.Collator("de-u-co-search", {usage: "sort"}).resolvedOptions().locale, "de");
shouldBe(new Intl.Collator("de-u-co-sort", {usage: "search"}).resolvedOptions().locale, "de");

shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de-u-kn", {usage: "sort"}).compare), ["\u00C4", "AE"]);
shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de-u-kn", {usage: "search"}).compare), ["AE", "\u00C4"]);
shouldBeArray(["2", "10"].sort(new Intl.Collator("de-u-kn", {usage: "sort"}).compare), ["2", "10"]);
shouldBeArray(["2", "10"].sort(new Intl.Collator("de-u-kn", {usage: "search"}).compare), ["2", "10"]);
shouldBeArray(["2", "10"].sort(new Intl.Collator("de-U-kn", {usage: "sort"}).compare), ["2", "10"]);
shouldBeArray(["2", "10"].sort(new Intl.Collator("de-U-kn-x-0", {usage: "search"}).compare), ["2", "10"]);

shouldBe(new Intl.Collator("en-US-x-twain", {usage: "search"}).resolvedOptions().locale, "en-US");

shouldBe(new Intl.Collator("de-u-kn", {usage: "sort"}).resolvedOptions().locale, "de-u-kn");
shouldBe(new Intl.Collator("de-u-kn", {usage: "search"}).resolvedOptions().locale, "de-u-kn");

shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de-u-co-phonebk").compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de", { collation: 'phonebk' }).compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de").compare), ["a", "ä", "ae", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de-u-co-phonebk", { usage: 'search' }).compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de", { usage: 'search', collation: 'phonebk' }).compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de", { usage: 'search' }).compare), ["a", "ae", "ä", "æ"]);

// We cannot set sort / search / standard though "collation" option. Use "usage" instead.
shouldBe(new Intl.Collator("de-u-kn", {collation: "search"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-kn", {collation: "sort"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-kn", {collation: "standard"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-kn", {usage: "search", collation: "search"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-kn", {usage: "search", collation: "sort"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-kn", {usage: "search", collation: "standard"}).resolvedOptions().collation, "default");

shouldThrow(() => {
    new Intl.Collator("de-u-kn", {"collation": "phonebook"});
}, `RangeError: collation is not a well-formed collation value`);

shouldBe(new Intl.Collator("de-u-kn", {collation: "phonebk"}).resolvedOptions().collation, "phonebk");
shouldBe(new Intl.Collator("de-u-kn", {usage: "sort", collation: "phonebk"}).resolvedOptions().collation, "phonebk");
shouldBe(new Intl.Collator("de-u-kn", {usage: "search", collation: "phonebk"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-co-dict", {collation: "phonebk"}).resolvedOptions().collation, "phonebk");
shouldBe(new Intl.Collator("de-u-co-dict", {collation: "phonebk"}).resolvedOptions().locale, "de");
shouldBe(new Intl.Collator("de-u-co-dict", {usage: "search", collation: "phonebk"}).resolvedOptions().collation, "default");
shouldBe(new Intl.Collator("de-u-co-dict", {usage: "search", collation: "phonebk"}).resolvedOptions().locale, "de");
