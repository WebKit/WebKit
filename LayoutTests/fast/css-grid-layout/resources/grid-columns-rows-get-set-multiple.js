description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting |grid-template-columns| and |grid-template-rows| set through CSS");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "7px 11px", "17px 2px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "50% 100%", "27% 52%", "400px 800px", "162px 312px");
testGridTemplatesValues(document.getElementById("gridWithPercentWithoutSize"), "50% 100%", "27% 52%", "3.5px 7px", "11px 0px");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "auto auto", "auto auto", "0px 17px", "0px 3px");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px 120px", "150px 170px");
testGridTemplatesValues(document.getElementById("gridWithThreeItems"), "15px auto 100px", "120px 18px auto", "15px 0px 100px", "120px 18px 0px");
testGridTemplatesValues(document.getElementById("gridWithPercentAndViewportPercent"), "50% 120px", "35% 168px", "400px 120px", "210px 168px");
testGridTemplatesValues(document.getElementById("gridWithFitContentAndFitAvailable"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithMinMaxContent"), "min-content max-content", "max-content min-content", "0px 0px", "0px 0px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxContentWithChildrenElement"), "min-content max-content", "max-content min-content", "7px 17px", "11px 3px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndFixed"), "minmax(45px, 30%) 15px", "120px minmax(35%, 10px)", "240px 15px", "120px 210px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndMinMaxContent"), "minmax(min-content, 30%) 15px", "120px minmax(35%, max-content)", "240px 15px", "120px 210px");
testGridTemplatesValues(document.getElementById("gridWithFractionFraction"), "1fr 2fr", "3fr 4fr", "320px 480px", "225px 375px");
testGridTemplatesValues(document.getElementById("gridWithFractionMinMax"), "minmax(min-content, 45px) 2fr", "3fr minmax(14px, max-content)", "45px 755px", "586px 14px");
testGridTemplatesValues(document.getElementById("gridWithCalcCalc"), "200px 100px", "150px 75px");
testGridTemplatesValues(document.getElementById("gridWithCalcAndFixed"), "50% 80px", "88px 25%", "400px 80px", "88px 150px");
testGridTemplatesValues(document.getElementById("gridWithCalcAndMinMax"), "190px minmax(min-content, 80px)", "minmax(25%, max-content) 53px", "190px 80px", "150px 53px");
testGridTemplatesValues(document.getElementById("gridWithCalcInsideMinMax"), "minmax(103px, 400px) 120px", "150px minmax(5%, 175px)", "400px 120px", "150px 175px");
testGridTemplatesValues(document.getElementById("gridWithAutoInsideMinMax"), "0px 30px", "132px 60px");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-rows and grid-template-columns through JS");
testGridTemplatesSetJSValues("18px 22px", "66px 70px");
testGridTemplatesSetJSValues("55% 80%", "40% 63%", "440px 640px", "240px 378px");
testGridTemplatesSetJSValues("auto auto", "auto auto", "0px 0px", "0px 0px");
testGridTemplatesSetJSValues("auto 16em 22px", "56% 10em auto", "0px 160px 22px", "336px 100px 0px");
testGridTemplatesSetJSValues("16em minmax(16px, 20px)", "minmax(10%, 15%) auto", "160px 20px", "90px 0px");
testGridTemplatesSetJSValues("16em 2fr", "14fr auto", "160px 640px", "600px 0px");
testGridTemplatesSetJSValues("calc(25px) calc(2em)", "auto calc(10%)", "25px 20px", "0px 60px", "calc(25px) calc(2em)", "auto calc(10%)");
testGridTemplatesSetJSValues("calc(25px + 40%) minmax(min-content, calc(10% + 12px))", "minmax(calc(75% - 350px), max-content) auto", "345px 92px", "100px 0px", "calc(25px + 40%) minmax(min-content, calc(10% + 12px))", "minmax(calc(75% - 350px), max-content) auto");
testGridTemplatesSetJSValues("auto minmax(16px, auto)", "minmax(auto, 15%) 10vw", "0px 16px", "90px 80px");

debug("");
debug("Test getting wrong values set from CSS");
testGridTemplatesValues(document.getElementById("gridWithNoneAndAuto"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithNoneAndFixed"), "none", "none");

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
// Invalid expressions with calc
testGridTemplatesSetBadJSValues("10px calc(16px 30px)", "calc(25px + auto) 2em");
testGridTemplatesSetBadJSValues("minmax(min-content, calc() 250px", "calc(2em(");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.width = "800px";
    parentElement.style.font = "10px Ahem"; // Used to resolve em font consistently.
    parentElement.style.gridTemplateColumns = "50px 1fr [last]";
    parentElement.style.gridTemplateRows = "2em [middle] 45px";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.display = "grid";
    element.style.gridTemplateColumns = "inherit";
    element.style.gridTemplateRows = "inherit";
    testGridTemplatesValues(element, "50px 750px [last]", "20px [middle] 45px");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.display = "grid";
    element.style.width = "800px";
    element.style.height = "600px";
    element.style.gridTemplateColumns = "150% [middle] 55px";
    element.style.gridTemplateRows = "1fr [line] 2fr [line]";
    testGridTemplatesValues(element, "1200px [middle] 55px", "200px [line] 400px [line]");

    element.style.gridTemplateColumns = "initial";
    element.style.gridTemplateRows = "initial";
    testGridTemplatesValues(element, "none", "none");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
