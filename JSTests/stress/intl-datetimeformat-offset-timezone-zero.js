function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "+00:00" }).resolvedOptions().timeZone, "+00:00");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "-00:00" }).resolvedOptions().timeZone, "+00:00");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "+00" }).resolvedOptions().timeZone, "+00:00");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "-00" }).resolvedOptions().timeZone, "+00:00");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "+0000" }).resolvedOptions().timeZone, "+00:00");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "UTC" }).resolvedOptions().timeZone, "UTC");
shouldBe(new Intl.DateTimeFormat(undefined, { timeZone: "+03:00" }).resolvedOptions().timeZone, "+03:00");
