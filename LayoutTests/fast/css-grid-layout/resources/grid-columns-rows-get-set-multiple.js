description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting |grid-template-columns| and |grid-template-rows| set through CSS");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "7px 11px", "17px 2px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "53% 99%", "27% 52%", "424px 792px", "162px 312px");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "auto auto", "auto auto", "0px 17px", "0px 3px");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px 120px", "150px 170px");
testGridTemplatesValues(document.getElementById("gridWithThreeItems"), "15px auto 100px", "120px 18px auto", "15px 0px 100px", "120px 18px 0px");
testGridTemplatesValues(document.getElementById("gridWithPercentAndViewportPercent"), "50% 120px", "35% 168px", "400px 120px", "210px 168px");
testGridTemplatesValues(document.getElementById("gridWithFitContentAndFitAvailable"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithMinMaxContent"), "-webkit-min-content -webkit-max-content", "-webkit-max-content -webkit-min-content", "0px 0px", "0px 0px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxContentWithChildrenElement"), "-webkit-min-content -webkit-max-content", "-webkit-max-content -webkit-min-content", "7px 17px", "11px 3px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndFixed"), "minmax(45px, 30%) 15px", "120px minmax(35%, 10px)", "240px 15px", "120px 210px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxAndMinMaxContent"), "minmax(-webkit-min-content, 30%) 15px", "120px minmax(35%, -webkit-max-content)", "240px 15px", "120px 210px");
testGridTemplatesValues(document.getElementById("gridWithFractionFraction"), "1fr 2fr", "3fr 4fr", "320px 480px", "225px 375px");
testGridTemplatesValues(document.getElementById("gridWithFractionMinMax"), "minmax(-webkit-min-content, 45px) 2fr", "3fr minmax(14px, -webkit-max-content)", "45px 755px", "586px 14px");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-rows and grid-template-columns through JS");
testGridTemplatesSetJSValues("18px 22px", "66px 70px");
testGridTemplatesSetJSValues("55% 80%", "40% 63%", "440px 640px", "240px 378px");
testGridTemplatesSetJSValues("auto auto", "auto auto", "0px 0px", "0px 0px");
testGridTemplatesSetJSValues("auto 16em 22px", "56% 10em auto", "0px 160px 22px", "336px 100px 0px");
testGridTemplatesSetJSValues("16em minmax(16px, 20px)", "minmax(10%, 15%) auto", "160px 20px", "90px 0px");
testGridTemplatesSetJSValues("16em 2fr", "14fr auto", "160px 640px", "600px 0px");

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
testGridTemplatesSetBadJSValues("-webkit-fit-content -webkit-fit-content", "-webkit-fit-available -webkit-fit-available");
testGridTemplatesSetBadJSValues("auto minmax(16px, auto)", "minmax(auto, 15%) 10vw");

// Negative values are not allowed.
testGridTemplatesSetBadJSValues("-10px minmax(16px, 32px)", "minmax(10%, 15%) -10vw");
testGridTemplatesSetBadJSValues("10px minmax(16px, -1vw)", "minmax(-1%, 15%) 10vw");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.width = "800px";
    parentElement.style.font = "10px Ahem"; // Used to resolve em font consistently.
    parentElement.style.webkitGridTemplateColumns = "50px 1fr 'last'";
    parentElement.style.webkitGridTemplateRows = "2em 'middle' 45px";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.display = "-webkit-grid";
    element.style.webkitGridTemplateColumns = "inherit";
    element.style.webkitGridTemplateRows = "inherit";
    testGridTemplatesValues(element, "50px 750px last", "20px middle 45px");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.display = "-webkit-grid";
    element.style.width = "800px";
    element.style.height = "600px";
    element.style.webkitGridTemplateColumns = "150% 'middle' 55px";
    element.style.webkitGridTemplateRows = "1fr 'line' 2fr 'line'";
    testGridTemplatesValues(element, "1200px middle 55px", "200px line 400px line");

    element.style.webkitGridTemplateColumns = "initial";
    element.style.webkitGridTemplateRows = "initial";
    testGridTemplatesValues(element, "none", "none");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
