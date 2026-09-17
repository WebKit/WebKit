// Most of these selectors are also in WPT (css/selectors/featureless-006.html). What this adds is the
// JIT: each one is a selector the CSS selector JIT can compile, and the file is run with the JIT both
// enabled and disabled, so a regression in ElementRuleCollector::ruleMatches() shows up as a
// difference between the two expectations.
function hostMatches(selector)
{
    const host = document.createElement("div");
    host.className = "host";
    document.body.appendChild(host);
    // Without a rule mentioning :host, the bucket is never collected for the host and the selector
    // under test is never considered.
    host.attachShadow({ mode: "open" }).innerHTML =
        `<style>:is(:host) { --flag: y } ${selector} { --matched: yes }</style>`;
    const matched = getComputedStyle(host).getPropertyValue("--matched").trim() === "yes";
    host.remove();
    return matched;
}

debug("Selectors the JIT compiles that match a div, but must not match the featureless shadow host");
shouldBeFalse('hostMatches("*")');
shouldBeFalse('hostMatches("div")');
shouldBeFalse('hostMatches(".host")');
shouldBeFalse('hostMatches(":not(span)")');
shouldBeFalse('hostMatches(":not(aside)")');
shouldBeFalse('hostMatches(":not(.foo)")');
shouldBeFalse('hostMatches(":not(:hover)")');

debug("");
debug("Selectors that compile to an always-false stub, so the compiler never reaches the :host");
shouldBeFalse('hostMatches("[a^=\'\']:host")');
shouldBeFalse('hostMatches(":nth-child(0):host")');
shouldBeFalse('hostMatches(":nth-child(0 of :host)")');

debug("");
debug("Controls: the host still matches what it is allowed to");
shouldBeTrue('hostMatches(":host")');
shouldBeTrue('hostMatches(":not(:not(:host))")');
