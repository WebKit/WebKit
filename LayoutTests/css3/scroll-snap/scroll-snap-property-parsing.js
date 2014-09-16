description("Test the parsing of the -webkit-scroll-snap-* properties.");

var stylesheet, cssRule, declaration;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testScrollSnapRule(description, snapProperty, rule, expectedValue)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { -webkit-scroll-snap-" + snapProperty + ": " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);

    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "1");
    shouldBe("declaration.getPropertyValue('-webkit-scroll-snap-" + snapProperty + "')", "'" + expectedValue + "'");
}

testScrollSnapRule("inherited type", "type", "inherit", "inherit");
testScrollSnapRule("initial type", "type", "initial", "initial");

testScrollSnapRule("inherited points-x", "points-x", "inherit", "inherit");
testScrollSnapRule("initial points-x", "points-x", "initial", "initial");

testScrollSnapRule("inherited points-y", "points-y", "inherit", "inherit");
testScrollSnapRule("initial points-y", "points-y", "initial", "initial");

testScrollSnapRule("inherited destination", "destination", "inherit", "inherit");
testScrollSnapRule("initial destination", "destination", "initial", "initial");

testScrollSnapRule("inherited coordinate", "coordinate", "inherit", "inherit");
testScrollSnapRule("initial coordinate", "coordinate", "initial", "initial");

testScrollSnapRule("mandatory type", "type", "mandatory", "mandatory");
testScrollSnapRule("proximity type", "type", "proximity", "proximity");

testScrollSnapRule("element points along x axis", "points-x", "elements", "elements");
testScrollSnapRule("percentage points along x axis", "points-x", "100% 50%", "100% 50%");
testScrollSnapRule("pixel points along x axis", "points-x", "100px 50px", "100px 50px");
testScrollSnapRule("percentage points repeat along x axis", "points-x", "repeat(100%)", "repeat(100%)");
testScrollSnapRule("pixe points repeat along x axis", "points-x", "repeat(25px)", "repeat(25px)");
testScrollSnapRule("percentage points along x axis with percentage repeat", "points-x", "100% repeat(100%)", "100% repeat(100%)");
testScrollSnapRule("pixel points along x axis with percentage repeat", "points-x", "100px 50px repeat(25%)", "100px 50px repeat(25%)");
testScrollSnapRule("percentage points along x axis with pixel repeat", "points-x", "100% 50% repeat(40px)", "100% 50% repeat(40px)");
testScrollSnapRule("pixel points along x axis with pixel repeat", "points-x", "100px repeat(42px)", "100px repeat(42px)");

testScrollSnapRule("element points along y axis", "points-y", "elements", "elements");
testScrollSnapRule("percentage points along y axis", "points-y", "100% 50%", "100% 50%");
testScrollSnapRule("pixel points along y axis", "points-y", "100px 50px", "100px 50px");
testScrollSnapRule("percentage points repeat along y axis", "points-y", "repeat(100%)", "repeat(100%)");
testScrollSnapRule("pixe points repeat along y axis", "points-y", "repeat(25px)", "repeat(25px)");
testScrollSnapRule("percentage points along y axis with percentage repeat", "points-y", "100% repeat(100%)", "100% repeat(100%)");
testScrollSnapRule("pixel points along y axis with percentage repeat", "points-y", "100px 50px repeat(25%)", "100px 50px repeat(25%)");
testScrollSnapRule("percentage points along y axis with pixel repeat", "points-y", "100% 50% repeat(40px)", "100% 50% repeat(40px)");
testScrollSnapRule("pixel points along y axis with pixel repeat", "points-y", "100px repeat(42px)", "100px repeat(42px)");

testScrollSnapRule("em points along x axis with pixel repeat", "points-x", "100em repeat(42em)", "100em repeat(42em)");
testScrollSnapRule("mm along x axis with pixel repeat", "points-x", "100mm repeat(42mm)", "100mm repeat(42mm)");
testScrollSnapRule("in along x axis with pixel repeat", "points-x", "100in repeat(42in)", "100in repeat(42in)");
testScrollSnapRule("pt along x axis with pixel repeat", "points-x", "100pt repeat(42pt)", "100pt repeat(42pt)");

testScrollSnapRule("pixel/pixel destination", "destination", "10px 50px", "10px 50px");
testScrollSnapRule("pixel/percentage destination", "destination", "20px 40%", "20px 40%");
testScrollSnapRule("percentage/pixel destination", "destination", "0% 0px", "0% 0px");
testScrollSnapRule("percentage/percentage destination", "destination", "5% 100%", "5% 100%");

testScrollSnapRule("em/ex destination", "destination", "12em 16ex", "12em 16ex");
testScrollSnapRule("in/cm destination", "destination", "2in 5cm", "2in 5cm");

testScrollSnapRule("single pixel coordinate", "coordinate", "50px 100px", "50px 100px");
testScrollSnapRule("single percentage coordinate", "coordinate", "50% 100%", "50% 100%");
testScrollSnapRule("multiple pixel coordinates", "coordinate", "50px 100px 150px 100px 200px 100px", "50px 100px 150px 100px 200px 100px");
testScrollSnapRule("multiple percentage coordinates", "coordinate", "50% 100% 150% 100% 200% 100%", "50% 100% 150% 100% 200% 100%");

testScrollSnapRule("em/ex coordinate", "coordinate", "12em 16ex 16em 12ex", "12em 16ex 16em 12ex");
testScrollSnapRule("in/cm coordinate", "coordinate", "2in 5cm 5in 2cm", "2in 5cm 5in 2cm");

successfullyParsed = true;
