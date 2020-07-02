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

function explicitTrueBeforeICU67() {
    return $vm.icuVersion() < 67 ? '-true' : '';
}

shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de", {usage: "sort"}).compare), ["\u00C4", "AE"]);
shouldBeArray(["AE", "\u00C4"].sort(new Intl.Collator("de", {usage: "search"}).compare), ["AE", "\u00C4"]);
shouldBe(new Intl.Collator("de", {usage: "sort"}).resolvedOptions().locale, "de");
shouldBe(new Intl.Collator("de", {usage: "search"}).resolvedOptions().locale, "de");
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

shouldBe(new Intl.Collator("de-u-kn", {usage: "sort"}).resolvedOptions().locale, "de-u-kn" + explicitTrueBeforeICU67());
shouldBe(new Intl.Collator("de-u-kn", {usage: "search"}).resolvedOptions().locale, "de-u-kn" + explicitTrueBeforeICU67());

shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de-u-co-phonebk").compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de").compare), ["a", "ä", "ae", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de-u-co-phonebk", { usage: 'search' }).compare), ["a", "ae", "ä", "æ"]);
shouldBeArray(["a", "ae", "ä", "æ"].sort(new Intl.Collator("de", { usage: 'search' }).compare), ["a", "ae", "ä", "æ"]);
