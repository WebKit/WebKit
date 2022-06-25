description("Test the computed style of the scroll-snap-* properties.");

var stylesheet;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testComputedScrollSnapRule(description, snapProperty, rule, expected, expectedShorthands = {})
{
    debug("");
    debug(`${description}: \`${rule}\``);

    stylesheet.insertRule(`body { ${snapProperty} : ${rule}; }`, 0);

    shouldBe(`window.getComputedStyle(document.body).getPropertyValue('${snapProperty}')`, `'${expected}'`);
    for (let shorthand in expectedShorthands)
        shouldBe(`window.getComputedStyle(document.body).getPropertyValue('${snapProperty}-${shorthand}')`, `'${expectedShorthands[shorthand]}'`);
    stylesheet.deleteRule(0);
}

// Test the scroll-snap-type property
// Invalid declarations
testComputedScrollSnapRule("invalid snap type", "scroll-snap-type", "potato", "none");
testComputedScrollSnapRule("empty string for snap type", "scroll-snap-type", "", "none");
testComputedScrollSnapRule("too many values", "scroll-snap-type", "block mandatory proximity", "none");
testComputedScrollSnapRule("none following axis", "scroll-snap-type", "both none", "none");
testComputedScrollSnapRule("two axis values", "scroll-snap-type", "block inline", "none");
testComputedScrollSnapRule("two strictness values", "scroll-snap-type", "proximity mandatory", "none");
testComputedScrollSnapRule("axis following strictness", "scroll-snap-type", "mandatory inline", "none");
testComputedScrollSnapRule("only strictness", "scroll-snap-type", "mandatory", "none");
// Valid declarations
testComputedScrollSnapRule("initial value", "scroll-snap-type", "initial", "none");
testComputedScrollSnapRule("only axis", "scroll-snap-type", "both", "both");
testComputedScrollSnapRule("none", "scroll-snap-type", "none", "none");
testComputedScrollSnapRule("strictness following axis", "scroll-snap-type", "inline mandatory", "inline mandatory");

// Test the scroll-snap-align property
// Invalid declarations
testComputedScrollSnapRule("invalid snap align", "scroll-snap-align", "potato", "none");
testComputedScrollSnapRule("empty string", "scroll-snap-align", "", "none");
testComputedScrollSnapRule("too many values", "scroll-snap-align", "start center end", "none");
testComputedScrollSnapRule("invalid second value", "scroll-snap-align", "start wut", "none");
testComputedScrollSnapRule("invalid first value", "scroll-snap-align", "wat center", "none");
testComputedScrollSnapRule("one length", "scroll-snap-align", "10px", "none");
testComputedScrollSnapRule("two lengths", "scroll-snap-align", "10px 50px", "none");
// Valid declarations
testComputedScrollSnapRule("initial value", "scroll-snap-align", "initial", "none");
testComputedScrollSnapRule("single value", "scroll-snap-align", "start", "start");
testComputedScrollSnapRule("two values", "scroll-snap-align", "start end", "start end");

// Test the scroll-padding property
// Invalid declarations
testComputedScrollSnapRule("invalid scroll padding", "scroll-padding", "potato", "auto", { top: "auto", left: "auto", right: "auto", bottom: "auto" });
testComputedScrollSnapRule("empty string", "scroll-padding", "", "auto", { top: "auto", left: "auto", right: "auto", bottom: "auto" });
testComputedScrollSnapRule("too many values", "scroll-padding", "1px 2px 3px 4px 5px", "auto", { top: "auto", left: "auto", right: "auto", bottom: "auto" });
testComputedScrollSnapRule("attempt to use auto", "scroll-padding", "auto auto", "auto", { top: "auto", left: "auto", right: "auto", bottom: "auto" });
// Valid declarations
testComputedScrollSnapRule("single length", "scroll-padding", "10px", "10px", { top: "10px", left: "10px", right: "10px", bottom: "10px" });
testComputedScrollSnapRule("two percentages", "scroll-padding", "10% 20%", "10% 20%", { top: "10%", left: "20%", right: "20%", bottom: "10%" });
testComputedScrollSnapRule("three lengths", "scroll-padding", "1px 2px 3px", "1px 2px 3px", { top: "1px", left: "2px", right: "2px", bottom: "3px" });
testComputedScrollSnapRule("four values", "scroll-padding", "50px 10% 20% 50px", "50px 10% 20% 50px", { top: "50px", left: "50px", right: "10%", bottom: "20%" });
testComputedScrollSnapRule("calc expression", "scroll-padding", "calc(50px + 10%) 20px", "calc(10% + 50px) 20px", { top: "calc(10% + 50px)", left: "20px", right: "20px", bottom: "calc(10% + 50px)" });
testComputedScrollSnapRule("various units", "scroll-padding", "1em 5mm 2in 4cm", "16px 18.897638px 192px 151.181107px", { top: "16px", left: "151.181107px", right: "18.897638px", bottom: "192px" });
testComputedScrollSnapRule("subpixel values", "scroll-padding", "10.4375px 6.5px", "10.4375px 6.5px", { top: "10.4375px", left: "6.5px", right: "6.5px", bottom: "10.4375px" });

// Test the scroll-snap-margin property
// Invalid declarations
testComputedScrollSnapRule("invalid scroll margin", "scroll-snap-margin", "potato", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
testComputedScrollSnapRule("empty string", "scroll-snap-margin", "", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
testComputedScrollSnapRule("too many values", "scroll-snap-margin", "1px 2px 3px 4px 5px", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
testComputedScrollSnapRule("attempt to use auto", "scroll-snap-margin", "auto auto", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
testComputedScrollSnapRule("attempt to use percentage", "scroll-snap-margin", "25% 25%", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
testComputedScrollSnapRule("attempt to use calc", "scroll-snap-margin", "calc(25% + 10px)", "0px", { top: "0px", left: "0px", right: "0px", bottom: "0px" });
// Valid declarations
testComputedScrollSnapRule("single length", "scroll-snap-margin", "10px", "10px", { top: "10px", left: "10px", right: "10px", bottom: "10px" });
testComputedScrollSnapRule("two lengths", "scroll-snap-margin", "10px 20px", "10px 20px", { top: "10px", left: "20px", right: "20px", bottom: "10px" });
testComputedScrollSnapRule("three lengths", "scroll-snap-margin", "1px 2px 3px", "1px 2px 3px", { top: "1px", left: "2px", right: "2px", bottom: "3px" });
testComputedScrollSnapRule("four lengths", "scroll-snap-margin", "50px 10px 20px 50px", "50px 10px 20px 50px", { top: "50px", left: "50px", right: "10px", bottom: "20px" });
testComputedScrollSnapRule("various units", "scroll-snap-margin", "1em 5mm 2in 4cm", "16px 18.897638px 192px 151.181107px", { top: "16px", left: "151.181107px", right: "18.897638px", bottom: "192px" });
testComputedScrollSnapRule("subpixel values", "scroll-snap-margin", "10.4375px 6.5px", "10.4375px 6.5px", { top: "10.4375px", left: "6.5px", right: "6.5px", bottom: "10.4375px" });

successfullyParsed = true;
