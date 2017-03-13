description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting grid-template-columns and grid-template-rows set through CSS");
testGridTemplatesValues(document.getElementById("gridWithNoneElement"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "10px", "15px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "53%", "27%");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "auto", "auto");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px", "150px");
testGridTemplatesValues(document.getElementById("gridWithViewPortPercentageElement"), "64px", "60px");
testGridTemplatesValues(document.getElementById("gridWithMinMax"), "minmax(10%, 15px)", "minmax(20px, 50%)");
testGridTemplatesValues(document.getElementById("gridWithMinContent"), "min-content", "min-content");
testGridTemplatesValues(document.getElementById("gridWithMaxContent"), "max-content", "max-content");
testGridTemplatesValues(document.getElementById("gridWithFraction"), "1fr", "2fr");
testGridTemplatesValues(document.getElementById("gridWithCalc"), "150px", "75px");
testGridTemplatesValues(document.getElementById("gridWithCalcComplex"), "calc(50% + 150px)", "calc(65% + 75px)");
testGridTemplatesValues(document.getElementById("gridWithCalcInsideMinMax"), "minmax(10%, 15px)", "minmax(20px, 50%)");
testGridTemplatesValues(document.getElementById("gridWithCalcComplexInsideMinMax"), "minmax(10%, calc(50% + 15px))", "minmax(calc(20px + 10%), 50%)");
testGridTemplatesValues(document.getElementById("gridWithAutoInsideMinMax"), "minmax(auto, 20px)", "minmax(min-content, auto)");

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
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows through JS");
testNonGridTemplatesSetJSValues("18px", "66px");
testNonGridTemplatesSetJSValues("55%", "40%");
testNonGridTemplatesSetJSValues("auto", "auto");
testNonGridTemplatesSetJSValues("10vw", "25vh", "80px", "150px");
testNonGridTemplatesSetJSValues("min-content", "min-content");
testNonGridTemplatesSetJSValues("max-content", "max-content");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows to minmax() values through JS");
testNonGridTemplatesSetJSValues("minmax(55%, 45px)", "minmax(30px, 40%)");
testNonGridTemplatesSetJSValues("minmax(22em, 8vh)", "minmax(10vw, 5em)", "minmax(220px, 48px)", "minmax(80px, 50px)");
testNonGridTemplatesSetJSValues("minmax(min-content, 8vh)", "minmax(10vw, min-content)", "minmax(min-content, 48px)", "minmax(80px, min-content)");
testNonGridTemplatesSetJSValues("minmax(22em, max-content)", "minmax(max-content, 5em)", "minmax(220px, max-content)", "minmax(max-content, 50px)");
testNonGridTemplatesSetJSValues("minmax(min-content, max-content)", "minmax(max-content, min-content)");
// Unit comparison should be case-insensitive.
testNonGridTemplatesSetJSValues("3600Fr", "154fR", "3600fr", "154fr", "3600fr", "154fr");
// Float values are allowed.
testNonGridTemplatesSetJSValues("3.1459fr", "2.718fr");
// A leading '+' is allowed.
testNonGridTemplatesSetJSValues("+3fr", "+4fr", "3fr", "4fr", "3fr", "4fr");
testNonGridTemplatesSetJSValues("minmax(auto, 8vh)", "minmax(10vw, auto)", "minmax(auto, 48px)", "minmax(80px, auto)");
// 0fr is allowed.
testGridTemplatesSetJSValues(".0000fr", "0fr", "0px", "0px", "0fr", "0fr");

debug("");
debug("Test setting grid-template-columns and grid-template-rows to bad values through JS");
// No comma and only 1 argument provided.
testGridTemplatesSetBadJSValues("minmax(10px 20px)", "minmax(10px)")
// Nested minmax and only 2 arguments are allowed.
testGridTemplatesSetBadJSValues("minmax(minmax(10px, 20px), 20px)", "minmax(10px, 20px, 30px)");
// No breadth value and no comma.
testGridTemplatesSetBadJSValues("minmax()", "minmax(30px 30% 30em)");
// Flexible lengths are invalid on the min slot of minmax().
testGridTemplatesSetBadJSValues("minmax(0fr, 100px)", "minmax(.0fr, 200px)");
testGridTemplatesSetBadJSValues("minmax(1fr, 100px)", "minmax(2.5fr, 200px)");

testGridTemplatesSetBadJSValues("-2fr", "3ffr");
testGridTemplatesSetBadJSValues("-2.05fr", "+-3fr");
testGridTemplatesSetBadJSValues("7.-fr", "-8,0fr");
// A dimension doesn't allow spaces between the number and the unit.
testGridTemplatesSetBadJSValues("1r", "13 fr");
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
    parentElement.style.gridTemplateColumns = "50px [last]";
    parentElement.style.gridTemplateRows = "[first] 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridTemplateColumns = "inherit";
    element.style.gridTemplateRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'50px [last]'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'[first] 101%'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridTemplateColumns = "150% [last]";
    element.style.gridTemplateRows = "[first] 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'150% [last]'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'[first] 1fr'");

    element.style.gridTemplateColumns = "initial";
    element.style.gridTemplateRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
