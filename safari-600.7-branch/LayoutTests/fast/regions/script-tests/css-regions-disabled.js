description('Test that parsing of css regions related properties is disabled by default.');

if (window.testRunner)
    window.testRunner.overridePreference("WebKitCSSRegionsEnabled", "0");

function testWebKitFlowInto(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitFlowInto;
}

function testWebKitFlowFrom(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitFlowFrom;
}

function testWebKitRegionFragment(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitRegionFragment;
}

function testComputedStyleWebKitFlowInto(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-flow-into", value);
    var computedValue = getComputedStyle(div).getPropertyValue("-webkit-flow-into");
    document.body.removeChild(div);
    return computedValue;
}

function testComputedStyleWebKitFlowFrom(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-flow-from", value);
    var computedValue = getComputedStyle(div).getPropertyValue("-webkit-flow-from");
    document.body.removeChild(div);
    return computedValue;
}

function testComputedStyleWebKitRegionFragment(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-region-fragment", value);
    var computedValue = getComputedStyle(div).getPropertyValue("-webkit-region-fragment");
    document.body.removeChild(div);
    return computedValue;
}

shouldBeEqualToString('testWebKitFlowInto("-webkit-flow-into: none")', "");
shouldBeEqualToString('testWebKitFlowInto("-webkit-flow-into: first-flow")', "");
shouldBeEqualToString('testComputedStyleWebKitFlowInto("none")', "none");
shouldBeEqualToString('testComputedStyleWebKitFlowInto("first-flow")', "none");

shouldBeEqualToString('testWebKitFlowFrom("-webkit-flow-from: first-flow")', "");
shouldBeEqualToString('testWebKitFlowFrom("-webkit-flow-from: none")', "");
shouldBeEqualToString('testComputedStyleWebKitFlowFrom("first-flow")', "none");
shouldBeEqualToString('testComputedStyleWebKitFlowFrom("none")', "none");

shouldBeEqualToString('testWebKitRegionFragment("-webkit-region-fragment: auto")', "");
shouldBeEqualToString('testWebKitRegionFragment("-webkit-region-fragment: break")', "");
shouldBeEqualToString('testComputedStyleWebKitRegionFragment("auto")', "auto");
shouldBeEqualToString('testComputedStyleWebKitRegionFragment("break")', "auto");

// Test that region styling rules are not parsed.
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
var stylesheet = styleElement.sheet;
webkitRegionRuleIndex = -1;
try {
    webkitRegionRuleIndex = stylesheet.insertRule("@-webkit-region #region3 { p { color: red; } }");
} catch(err) {
}

shouldBe("webkitRegionRuleIndex", "-1");

