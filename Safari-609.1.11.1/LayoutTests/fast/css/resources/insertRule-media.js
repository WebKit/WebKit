description(
'Test parsing of @media rule using insertRule().  <a href="http://bugs.webkit.org/show_bug.cgi?id=15986">http://bugs.webkit.org/show_bug.cgi?id=15986</a>'
);

var rule = "@media all and (min-width: 0px) { \n  #greenid { color: green; }\n}";
var serializedRule = "@media (min-width: 0px) { \n  #greenid { color: green; }\n}";
shouldBe("document.styleSheets[0].insertRule(rule, 0)", "0");
shouldBe("document.styleSheets[0].rules[0].cssText", "serializedRule");
