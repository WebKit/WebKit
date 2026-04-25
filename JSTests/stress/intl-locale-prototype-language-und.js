function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}


var loc = new Intl.Locale("und");

shouldBe(loc.toString(), "und");
shouldBe(loc.baseName, "und");
shouldBe(loc.language, "und");
shouldBe(loc.script, undefined);
shouldBe(loc.region, undefined);
shouldBe(loc.variants, undefined);

var loc = new Intl.Locale("und-US-u-co-emoji");

shouldBe(loc.toString(), "und-US-u-co-emoji");
shouldBe(loc.baseName, "und-US");
shouldBe(loc.language, "und");
shouldBe(loc.script, undefined);
shouldBe(loc.region, "US");
shouldBe(loc.variants, undefined);
if ("collation" in loc) {
    shouldBe(loc.collation, "emoji");
}

