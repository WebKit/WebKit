// Spec coverage for featureless matching lives in WPT (css/selectors/featureless-00*). This tests
// only what is specific to WebKit: the shadow host must not be matched by the rule-hash bucket
// shortcut or by a compiled selector, neither of which implements featureless matching. Every
// selector here is one the CSS selector JIT can compile, and every one of them matched the host
// before webkit.org/b/283062. Run with the JIT both enabled and disabled, so that a regression in
// ElementRuleCollector::ruleMatches() shows up as a difference between the two.
function hostMatches(selector)
{
    const host = document.createElement("div");
    host.className = "host";
    document.body.appendChild(host);
    // The :is(:host) rule is load-bearing: without a rule mentioning :host, the universal bucket is
    // never collected for the host and the selector under test is never even considered.
    host.attachShadow({ mode: "open" }).innerHTML =
        `<style>:is(:host) { --flag: y } ${selector} { --matched: yes }</style>`;
    const matched = getComputedStyle(host).getPropertyValue("--matched").trim() === "yes";
    host.remove();
    return matched;
}

debug("Selectors that match a div, but must not match the featureless shadow host");
shouldBeFalse('hostMatches("*")');
shouldBeFalse('hostMatches("div")');
shouldBeFalse('hostMatches(".host")');
shouldBeFalse('hostMatches(":not(span)")');
shouldBeFalse('hostMatches(":not(aside)")');
shouldBeFalse('hostMatches(":not(.foo)")');
shouldBeFalse('hostMatches(":not(:hover)")');

debug("");
debug("Controls: the host still matches what it is allowed to");
shouldBeTrue('hostMatches(":host")');
shouldBeTrue('hostMatches(":not(:not(:host))")');
