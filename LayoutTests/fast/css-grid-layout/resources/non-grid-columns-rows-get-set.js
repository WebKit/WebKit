description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting grid-template-columns and grid-template-rows set through CSS");
testGridTemplatesValues(document.getElementById("gridWithNoneElement"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "10px", "15px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "53%", "27%");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "auto", "auto");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px", "150px");
testGridTemplatesValues(document.getElementById("gridWithViewPortPercentageElement"), "64px", "60px");
testGridTemplatesValues(document.getElementById("gridWithMinMax"), "minmax(10%, 15px)", "minmax(20px, 50%)");
testGridTemplatesValues(document.getElementById("gridWithMinContent"), "-webkit-min-content", "-webkit-min-content");
testGridTemplatesValues(document.getElementById("gridWithMaxContent"), "-webkit-max-content", "-webkit-max-content");
testGridTemplatesValues(document.getElementById("gridWithFraction"), "1fr", "2fr");

debug("");
debug("Test getting wrong values for grid-template-columns and grid-template-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
testGridTemplatesValues(gridWithFitContentElement, "none", "none");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
testGridTemplatesValues(gridWithFitAvailableElement, "none", "none");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
testGridTemplatesValues(element, "none", "none");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows through JS");
testNonGridTemplatesSetJSValues("18px", "66px");
testNonGridTemplatesSetJSValues("55%", "40%");
testNonGridTemplatesSetJSValues("auto", "auto");
testNonGridTemplatesSetJSValues("10vw", "25vh", "80px", "150px");
testNonGridTemplatesSetJSValues("-webkit-min-content", "-webkit-min-content");
testNonGridTemplatesSetJSValues("-webkit-max-content", "-webkit-max-content");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows to minmax() values through JS");
testNonGridTemplatesSetJSValues("minmax(55%, 45px)", "minmax(30px, 40%)");
testNonGridTemplatesSetJSValues("minmax(22em, 8vh)", "minmax(10vw, 5em)", "minmax(220px, 48px)", "minmax(80px, 50px)");
testNonGridTemplatesSetJSValues("minmax(-webkit-min-content, 8vh)", "minmax(10vw, -webkit-min-content)", "minmax(-webkit-min-content, 48px)", "minmax(80px, -webkit-min-content)");
testNonGridTemplatesSetJSValues("minmax(22em, -webkit-max-content)", "minmax(-webkit-max-content, 5em)", "minmax(220px, -webkit-max-content)", "minmax(-webkit-max-content, 50px)");
testNonGridTemplatesSetJSValues("minmax(-webkit-min-content, -webkit-max-content)", "minmax(-webkit-max-content, -webkit-min-content)");
// Unit comparison should be case-insensitive.
testNonGridTemplatesSetJSValues("3600Fr", "154fR", "3600fr", "154fr", "3600fr", "154fr");
// Float values are allowed.
testNonGridTemplatesSetJSValues("3.1459fr", "2.718fr");
// A leading '+' is allowed.
testNonGridTemplatesSetJSValues("+3fr", "+4fr", "3fr", "4fr", "3fr", "4fr");

debug("");
debug("Test setting grid-template-columns and grid-template-rows to bad values through JS");
// No comma and only 1 argument provided.
testGridTemplatesSetBadJSValues("minmax(10px 20px)", "minmax(10px)")
// Nested minmax and only 2 arguments are allowed.
testGridTemplatesSetBadJSValues("minmax(minmax(10px, 20px), 20px)", "minmax(10px, 20px, 30px)");
// No breadth value and no comma.
testGridTemplatesSetBadJSValues("minmax()", "minmax(30px 30% 30em)");
// Auto is not allowed inside minmax.
testGridTemplatesSetBadJSValues("minmax(auto, 8vh)", "minmax(10vw, auto)");
testGridTemplatesSetBadJSValues("-2fr", "3ffr");
testGridTemplatesSetBadJSValues("-2.05fr", "+-3fr");
testGridTemplatesSetBadJSValues("0fr", "1r");
// A dimension doesn't allow spaces between the number and the unit.
testGridTemplatesSetBadJSValues(".0000fr", "13 fr");
testGridTemplatesSetBadJSValues("7.-fr", "-8,0fr");
// Negative values are not allowed.
testGridTemplatesSetBadJSValues("-1px", "-6em");
testGridTemplatesSetBadJSValues("minmax(-1%, 32%)", "minmax(2vw, -6em)");

debug("");
debug("Test setting grid-template-columns and grid-template-rows back to 'none' through JS");
testNonGridTemplatesSetJSValues("18px", "66px");
testNonGridTemplatesSetJSValues("none", "none");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.webkitGridTemplateColumns = "50px 'last'";
    parentElement.style.webkitGridTemplateRows = "'first' 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.webkitGridTemplateColumns = "inherit";
    element.style.webkitGridTemplateRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'50px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'first 101%'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.webkitGridTemplateColumns = "150% 'last'";
    element.style.webkitGridTemplateRows = "'first' 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'150% last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'first 1fr'");

    element.style.webkitGridTemplateColumns = "initial";
    element.style.webkitGridTemplateRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
