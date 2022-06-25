description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting |grid-template-columns| and |grid-template-rows| set through CSS");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "7px 11px", "17px 2px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "53% 99%", "27% 52%");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "auto auto", "auto auto");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px 120px", "150px 170px");
testGridTemplatesValues(document.getElementById("gridWithThreeItems"), "15px auto 100px", "120px 18px auto");
testGridTemplatesValues(document.getElementById("gridWithPercentAndViewportPercent"), "50% 120px", "35% 168px");
testGridTemplatesValues(document.getElementById("gridWithFitContentAndFitAvailable"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithMinMaxContent"), "min-content max-content", "max-content min-content");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndFixed"), "minmax(45px, 30%) 15px", "120px minmax(35%, 10px)");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndMinMaxContent"), "minmax(min-content, 30%) 15px", "120px minmax(35%, max-content)");
testGridTemplatesValues(document.getElementById("gridWithFractionFraction"), "1fr 2fr", "3fr 4fr");
testGridTemplatesValues(document.getElementById("gridWithFractionMinMax"), "minmax(min-content, 45px) 2fr", "3fr minmax(14px, max-content)");
testGridTemplatesValues(document.getElementById("gridWithCalcCalc"), "200px 100px", "150px 75px");
testGridTemplatesValues(document.getElementById("gridWithCalcAndFixed"), "50% 80px", "88px 25%");
testGridTemplatesValues(document.getElementById("gridWithCalcAndMinMax"), "calc(20% + 30px) minmax(min-content, 80px)", "minmax(25%, max-content) calc(10% - 7px)");
testGridTemplatesValues(document.getElementById("gridWithCalcInsideMinMax"), "minmax(calc(10% + 23px), 400px) 120px", "150px minmax(5%, calc(50% - 125px))");
testGridTemplatesValues(document.getElementById("gridWithAutoInsideMinMax"), "minmax(auto, 20px) 10%", "max-content minmax(min-content, auto)");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-rows and grid-template-columns through JS");
testNonGridTemplatesSetJSValues("18px 22px", "66px 70px");
testNonGridTemplatesSetJSValues("55% 80%", "40% 63%");
testNonGridTemplatesSetJSValues("auto auto", "auto auto");
testNonGridTemplatesSetJSValues("auto 16em 22px", "56% 10em auto", "auto 160px 22px", "56% 100px auto");
testNonGridTemplatesSetJSValues("16em minmax(16px, 20px)", "minmax(10%, 15%) auto", "160px minmax(16px, 20px)");
testNonGridTemplatesSetJSValues("16em 2fr", "14fr auto", "160px 2fr");
testNonGridTemplatesSetJSValues("50% 12vw", "5% 85vh", "50% 96px", "5% 510px");
testNonGridTemplatesSetJSValues("auto minmax(16px, auto)", "minmax(auto, 15%) 10vw", "auto minmax(16px, auto)", "minmax(auto, 15%) 80px");

debug("");
debug("Test getting wrong values set from CSS");
var gridWithNoneAndAuto = document.getElementById("gridWithNoneAndAuto");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-template-rows')", "'none'");

var gridWithNoneAndFixed = document.getElementById("gridWithNoneAndFixed");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-template-rows')", "'none'");

debug("");
debug("Test setting and getting wrong values from JS");
testGridTemplatesSetBadJSValues("none auto", "none auto");
testGridTemplatesSetBadJSValues("none 16em", "none 56%");
testGridTemplatesSetBadJSValues("none none", "none none");
testGridTemplatesSetBadJSValues("auto none", "auto none");
testGridTemplatesSetBadJSValues("auto none 16em", "auto 18em none");
testGridTemplatesSetBadJSValues("fit-content fit-content", "-webkit-fit-available -webkit-fit-available");
// Negative values are not allowed.
testGridTemplatesSetBadJSValues("-10px minmax(16px, 32px)", "minmax(10%, 15%) -10vw");
testGridTemplatesSetBadJSValues("10px minmax(16px, -1vw)", "minmax(-1%, 15%) 10vw");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridTemplateColumns = "50px 1fr [last]";
    parentElement.style.gridTemplateRows = "101% [middle] 45px";
    testGridTemplatesValues(parentElement, "50px 1fr [last]", "101% [middle] 45px");

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridTemplateColumns = "inherit";
    element.style.gridTemplateRows = "inherit";
    testGridTemplatesValues(element, "50px 1fr [last]", "101% [middle] 45px");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridTemplateColumns = "150% [middle] 55px";
    element.style.gridTemplateRows = "1fr [line] 2fr [line]";
    testGridTemplatesValues(element, "150% [middle] 55px", "1fr [line] 2fr [line]");

    element.style.gridTemplateColumns = "initial";
    element.style.gridTemplateRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
