//@ runDefault("--useDollarVM=1")

if (typeof $vm === "undefined" || typeof $vm.setUserPreferredLanguages !== "function")
    quit();

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: bad value: ${actual}, expected: ${expected}`);
}

// "xx" is structurally valid but not an available locale, so it resolves via the default locale.
// The cached String#localeCompare collator must not survive a default-locale change.
$vm.setUserPreferredLanguages(["en-US"]);

setTimeout(() => {
    for (let i = 0; i < 3; ++i)
        shouldBe("ö".localeCompare("z", "xx"), new Intl.Collator("xx").compare("ö", "z"), "en-US");
    shouldBe("ö".localeCompare("z", "xx") < 0, true, "en-US ordering");

    $vm.setUserPreferredLanguages(["sv-SE"]);

    setTimeout(() => {
        shouldBe(new Intl.Collator("xx").compare("ö", "z") > 0, true, "sv-SE ordering via fresh collator");
        for (let i = 0; i < 3; ++i)
            shouldBe("ö".localeCompare("z", "xx"), new Intl.Collator("xx").compare("ö", "z"), "sv-SE");
    }, 0);
}, 0);
