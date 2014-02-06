description('Test that setting and getting grid-definition-columns and grid-definition-rows works as expected');

debug("Test getting grid-definition-columns and grid-definition-rows set through CSS");
testGridDefinitionsValues(document.getElementById("gridWithNoneElement"), "none", "none");
testGridDefinitionsValues(document.getElementById("gridWithFixedElement"), "10px", "15px");
testGridDefinitionsValues(document.getElementById("gridWithPercentElement"), "53%", "27%");
testGridDefinitionsValues(document.getElementById("gridWithAutoElement"), "auto", "auto");
testGridDefinitionsValues(document.getElementById("gridWithEMElement"), "100px", "150px");
testGridDefinitionsValues(document.getElementById("gridWithViewPortPercentageElement"), "64px", "60px");
testGridDefinitionsValues(document.getElementById("gridWithMinMax"), "minmax(10%, 15px)", "minmax(20px, 50%)");
testGridDefinitionsValues(document.getElementById("gridWithMinContent"), "-webkit-min-content", "-webkit-min-content");
testGridDefinitionsValues(document.getElementById("gridWithMaxContent"), "-webkit-max-content", "-webkit-max-content");
testGridDefinitionsValues(document.getElementById("gridWithFraction"), "1fr", "2fr");

debug("");
debug("Test getting wrong values for grid-definition-columns and grid-definition-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
testGridDefinitionsValues(gridWithFitContentElement, "none", "none");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
testGridDefinitionsValues(gridWithFitAvailableElement, "none", "none");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
testGridDefinitionsValues(element, "none", "none");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test getting and setting grid-definition-columns and grid-definition-rows through JS");
testNonGridDefinitionsSetJSValues("18px", "66px");
testNonGridDefinitionsSetJSValues("55%", "40%");
testNonGridDefinitionsSetJSValues("auto", "auto");
testNonGridDefinitionsSetJSValues("10vw", "25vh", "80px", "150px");
testNonGridDefinitionsSetJSValues("-webkit-min-content", "-webkit-min-content");
testNonGridDefinitionsSetJSValues("-webkit-max-content", "-webkit-max-content");

debug("");
debug("Test getting and setting grid-definition-columns and grid-definition-rows to minmax() values through JS");
testNonGridDefinitionsSetJSValues("minmax(55%, 45px)", "minmax(30px, 40%)");
testNonGridDefinitionsSetJSValues("minmax(22em, 8vh)", "minmax(10vw, 5em)", "minmax(220px, 48px)", "minmax(80px, 50px)");
testNonGridDefinitionsSetJSValues("minmax(-webkit-min-content, 8vh)", "minmax(10vw, -webkit-min-content)", "minmax(-webkit-min-content, 48px)", "minmax(80px, -webkit-min-content)");
testNonGridDefinitionsSetJSValues("minmax(22em, -webkit-max-content)", "minmax(-webkit-max-content, 5em)", "minmax(220px, -webkit-max-content)", "minmax(-webkit-max-content, 50px)");
testNonGridDefinitionsSetJSValues("minmax(-webkit-min-content, -webkit-max-content)", "minmax(-webkit-max-content, -webkit-min-content)");
// Unit comparison should be case-insensitive.
testNonGridDefinitionsSetJSValues("3600Fr", "154fR", "3600fr", "154fr", "3600fr", "154fr");
// Float values are allowed.
testNonGridDefinitionsSetJSValues("3.1459fr", "2.718fr");
// A leading '+' is allowed.
testNonGridDefinitionsSetJSValues("+3fr", "+4fr", "3fr", "4fr", "3fr", "4fr");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to bad values through JS");
// No comma and only 1 argument provided.
testGridDefinitionsSetBadJSValues("minmax(10px 20px)", "minmax(10px)")
// Nested minmax and only 2 arguments are allowed.
testGridDefinitionsSetBadJSValues("minmax(minmax(10px, 20px), 20px)", "minmax(10px, 20px, 30px)");
// No breadth value and no comma.
testGridDefinitionsSetBadJSValues("minmax()", "minmax(30px 30% 30em)");
// Auto is not allowed inside minmax.
testGridDefinitionsSetBadJSValues("minmax(auto, 8vh)", "minmax(10vw, auto)");
testGridDefinitionsSetBadJSValues("-2fr", "3ffr");
testGridDefinitionsSetBadJSValues("-2.05fr", "+-3fr");
testGridDefinitionsSetBadJSValues("0fr", "1r");
// A dimension doesn't allow spaces between the number and the unit.
testGridDefinitionsSetBadJSValues(".0000fr", "13 fr");
testGridDefinitionsSetBadJSValues("7.-fr", "-8,0fr");
// Negative values are not allowed.
testGridDefinitionsSetBadJSValues("-1px", "-6em");
testGridDefinitionsSetBadJSValues("minmax(-1%, 32%)", "minmax(2vw, -6em)");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows back to 'none' through JS");
testNonGridDefinitionsSetJSValues("18px", "66px");
testNonGridDefinitionsSetJSValues("none", "none");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.webkitGridDefinitionColumns = "50px 'last'";
    parentElement.style.webkitGridDefinitionRows = "'first' 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.webkitGridDefinitionColumns = "inherit";
    element.style.webkitGridDefinitionRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'50px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'first 101%'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.webkitGridDefinitionColumns = "150% 'last'";
    element.style.webkitGridDefinitionRows = "'first' 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'150% last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'first 1fr'");

    element.style.webkitGridDefinitionColumns = "initial";
    element.style.webkitGridDefinitionRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'initial' through JS");
testInitial();
