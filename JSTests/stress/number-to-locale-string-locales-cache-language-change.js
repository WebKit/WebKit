//@ runDefault("--useDollarVM=1")

if (typeof $vm === "undefined" || typeof $vm.setUserPreferredLanguages !== "function")
    quit();

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

$vm.setUserPreferredLanguages(["en-US"]);

setTimeout(() => {
    for (let i = 0; i < 3; ++i) {
        shouldBe((1234.5).toLocaleString("xx"), "1,234.5");
        shouldBe(1234n.toLocaleString("xx"), "1,234");
    }

    $vm.setUserPreferredLanguages(["de-DE"]);

    for (let i = 0; i < 3; ++i) {
        shouldBe((1234.5).toLocaleString("xx"), "1.234,5");
        shouldBe(1234n.toLocaleString("xx"), "1.234");
    }

    setTimeout(() => {
        shouldBe(new Intl.NumberFormat("xx").format(1234.5), "1.234,5");
        for (let i = 0; i < 3; ++i) {
            shouldBe((1234.5).toLocaleString("xx"), "1.234,5");
            shouldBe(1234n.toLocaleString("xx"), "1.234");
        }
    }, 0);
}, 0);
