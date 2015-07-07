description(
'This test checks that the CSSRule RuleTypes for keyframe-related rules are what we expect.'
);

var ruleType = window.CSSRule.WEBKIT_KEYFRAMES_RULE;
shouldBe("ruleType", "7");
ruleType = window.CSSRule.WEBKIT_KEYFRAME_RULE;
shouldBe("ruleType", "8");

debug('If we got to this point then we did not crash and the test has passed.');
var successfullyParsed = true;
