description("Test the parsing of the scroll-snap-* properties.");

var stylesheet, cssRule, declaration;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testScrollSnapRule(description, snapProperty, rule, expectedValue, expectedLength)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule(`body { ${snapProperty} : ${rule}; }`, 0);
    cssRule = stylesheet.cssRules.item(0);

    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", `${expectedLength}`);
    shouldBe(`declaration.getPropertyValue('${snapProperty}')`, `'${expectedValue}'`);
}

testScrollSnapRule("initial value", "scroll-snap-type", "initial", "initial", 1);
testScrollSnapRule("only strictness", "scroll-snap-type", "mandatory", "mandatory", 1);
testScrollSnapRule("only axis", "scroll-snap-type", "both", "both", 1);
testScrollSnapRule("none", "scroll-snap-type", "none", "none", 1);
testScrollSnapRule("strictness following axis", "scroll-snap-type", "inline mandatory", "inline mandatory", 1);
testScrollSnapRule("initial value", "scroll-snap-align", "initial", "initial", 1);
testScrollSnapRule("single value", "scroll-snap-align", "start", "start", 1);
testScrollSnapRule("two values", "scroll-snap-align", "start end", "start end", 1);
testScrollSnapRule("single length", "scroll-padding", "10px", "10px", 4);
testScrollSnapRule("two percentages", "scroll-padding", "10% 20%", "10% 20%", 4);
testScrollSnapRule("three lengths", "scroll-padding", "1px 2px 3px", "1px 2px 3px", 4);
testScrollSnapRule("four values", "scroll-padding", "50px 10% 20% 50px", "50px 10% 20% 50px", 4);
testScrollSnapRule("calc expression", "scroll-padding", "calc(50px + 10%) 20px", "calc(50px + 10%) 20px", 4);
testScrollSnapRule("various units", "scroll-padding", "1em 5mm 2in 4cm", "1em 5mm 2in 4cm", 4);
testScrollSnapRule("subpixel values", "scroll-padding", "10.4375px 6.5px", "10.4375px 6.5px", 4);
testScrollSnapRule("single length", "scroll-snap-margin", "10px", "10px", 4);
testScrollSnapRule("two lengths", "scroll-snap-margin", "10px 20px", "10px 20px", 4);
testScrollSnapRule("three lengths", "scroll-snap-margin", "1px 2px 3px", "1px 2px 3px", 4);
testScrollSnapRule("four lengths", "scroll-snap-margin", "50px 10px 20px 50px", "50px 10px 20px 50px", 4);
testScrollSnapRule("various units", "scroll-snap-margin", "1em 5mm 2in 4cm", "1em 5mm 2in 4cm", 4);
testScrollSnapRule("subpixel values", "scroll-snap-margin", "10.4375px 6.5px", "10.4375px 6.5px", 4);

successfullyParsed = true;
