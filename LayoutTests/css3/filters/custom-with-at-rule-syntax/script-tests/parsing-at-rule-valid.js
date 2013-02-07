// Requires custom-filter-parsing-common.js.

description("Test at-rule parsing for @-webkit-filter.");

// These have to be global for the test helpers to see them.
var cssRule, declaration;

function testFilterAtRule(description, rule, expectedValue, expectedProperties)
{
    debug("\n" + description + "\n" + rule);

    stylesheet.insertRule(rule, 0);

    // Check the rule's text and type.
    cssRule = stylesheet.cssRules.item(0);
    checkRule(expectedValue, "CSSRule.WEBKIT_FILTER_RULE", "WebKitCSSFilterRule");

    // Check the rule's CSSStyleDeclaration properties.
    declaration = cssRule.style;
    if (!expectedProperties)
        expectedProperties = [];
    var numProperties = 0;
    for (var propertyName in expectedProperties) {
        var propertyValue = expectedProperties[propertyName];
        shouldBeEqualToString("declaration.getPropertyValue('" + propertyName + "')", propertyValue);
        numProperties++;
    }
    shouldEvaluateTo("declaration.length", numProperties);
}

function testNestedRules(description, parentRule, parentExpectations, childExpectations)
{
    debug("\n" + description + "\n" + parentRule);

    stylesheet.insertRule(parentRule, 0);

    cssRule = stylesheet.cssRules.item(0);
    checkRule(parentExpectations.cssText, parentExpectations.ruleType, parentExpectations.constructorName);

    if (childExpectations) {
        cssRule = cssRule.cssRules[0];
        checkRule(childExpectations.cssText, childExpectations.ruleType, childExpectations.constructorName);

        for (var i = 0; i < cssRule.cssText.length; i++) {
            if (cssRule.cssText[i] != childExpectations.cssText[i])
                window.alert(i + ": " + cssRule.cssText[i]);
        }
    }
}

// Checks the global "cssRule" against some expected properties.
function checkRule(expectedCSSText, expectedRuleType, expectedConstructorName)
{
    shouldBeEqualToString("cssRule.cssText", expectedCSSText);
    shouldEvaluateTo("cssRule.type", expectedRuleType);
    shouldHaveConstructor("cssRule", expectedConstructorName);
}

heading("Filter at-rule tests.");
testFilterAtRule("Empty rule, separated by single spaces.",
    "@-webkit-filter my-filter { }",
    "@-webkit-filter my-filter { }");
testFilterAtRule("Empty rule, separated by multiple spaces.",
    "   @-webkit-filter   my-filter   {   }   ",
    "@-webkit-filter my-filter { }");
testFilterAtRule("Empty rule, no extra whitespace.",
    "@-webkit-filter my-filter{}",
    "@-webkit-filter my-filter { }");
testFilterAtRule("Rule with arbitrary properties.",
    "@-webkit-filter my-filter { width: 100px; height: 100px; }", 
    "@-webkit-filter my-filter { width: 100px; height: 100px; }",
    {width: "100px", height: "100px"});

heading("Nested filter at-rule tests.");
testNestedRules("Nested rule.",
    "@-webkit-filter parent-filter { @-webkit-filter child-filter { } }", 
    {
        cssText: "@-webkit-filter parent-filter { }",
        ruleType: CSSRule.WEBKIT_FILTER_RULE,
        constructorName: "WebKitCSSFilterRule"
    });
testNestedRules("Twice nested rule.",
    "@-webkit-filter parent-filter { @-webkit-filter child-filter { @-webkit-filter grandchild-filter } }", 
    {
        cssText: "@-webkit-filter parent-filter { }",
        ruleType: CSSRule.WEBKIT_FILTER_RULE,
        constructorName: "WebKitCSSFilterRule"
    });
testNestedRules("Nested rule inside arbitrary rule.", 
    "@font-face { @-webkit-filter child-filter { } }", 
    {
        cssText: "@font-face { }",
        ruleType: CSSRule.FONT_FACE_RULE,
        constructorName: "CSSFontFaceRule"
    });
testNestedRules("Nested rule inside media query.",
    "@media screen { @-webkit-filter child-filter { } }", 
    {
        cssText: "@media screen { \n  @-webkit-filter child-filter { }\n}",
        ruleType: CSSRule.MEDIA_RULE,
        constructorName: "CSSMediaRule",
    },
    {
        cssText: "@-webkit-filter child-filter { }",
        ruleType: CSSRule.WEBKIT_FILTER_RULE,
        constructorName: "WebKitCSSFilterRule",
    });
