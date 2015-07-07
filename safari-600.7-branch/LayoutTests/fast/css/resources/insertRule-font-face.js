description(
'Test parsing of @font-face rule using insertRule().  <a href="http://bugs.webkit.org/show_bug.cgi?id=15986">http://bugs.webkit.org/show_bug.cgi?id=15986</a>'
);

var rule = "@font-face { }";
shouldBe("document.styleSheets[0].insertRule(rule, 0)", "0");
shouldBe("document.styleSheets[0].rules[0].cssText", "rule");
