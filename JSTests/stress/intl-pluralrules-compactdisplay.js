function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// resolvedOptions should expose compactDisplay only when notation is "compact".
{
    let pr = new Intl.PluralRules("en", { notation: "compact", compactDisplay: "long" });
    let resolved = pr.resolvedOptions();
    shouldBe(resolved.notation, "compact");
    shouldBe(resolved.compactDisplay, "long");
}
{
    let pr = new Intl.PluralRules("en", { notation: "compact", compactDisplay: "short" });
    let resolved = pr.resolvedOptions();
    shouldBe(resolved.notation, "compact");
    shouldBe(resolved.compactDisplay, "short");
}
{
    let pr = new Intl.PluralRules("en", { notation: "compact" });
    let resolved = pr.resolvedOptions();
    shouldBe(resolved.notation, "compact");
    shouldBe(resolved.compactDisplay, "short");
}
{
    let pr = new Intl.PluralRules("en", { notation: "standard", compactDisplay: "long" });
    let resolved = pr.resolvedOptions();
    shouldBe(resolved.notation, "standard");
    shouldBe("compactDisplay" in resolved, false);
}
{
    let pr = new Intl.PluralRules("en");
    let resolved = pr.resolvedOptions();
    shouldBe(resolved.notation, "standard");
    shouldBe("compactDisplay" in resolved, false);
}

// resolvedOptions property order: compactDisplay must come right after notation.
{
    let resolved = new Intl.PluralRules("en", { notation: "compact", compactDisplay: "long" }).resolvedOptions();
    let keys = Object.keys(resolved);
    shouldBe(keys.indexOf("compactDisplay"), keys.indexOf("notation") + 1);
}
