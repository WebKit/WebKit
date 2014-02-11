description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting -webkit-grid-template-columns and -webkit-grid-template-rows set through CSS");
testGridTemplatesValues(document.getElementById("gridWithNoneElement"), "none", "none");
testGridTemplatesValues(document.getElementById("gridWithFixedElement"), "10px", "15px");
testGridTemplatesValues(document.getElementById("gridWithPercentElement"), "424px", "162px");
testGridTemplatesValues(document.getElementById("gridWithAutoElement"), "0px", "0px");
testGridTemplatesValues(document.getElementById("gridWithAutoWithChildrenElement"), "7px", "11px");
testGridTemplatesValues(document.getElementById("gridWithEMElement"), "100px", "150px");
testGridTemplatesValues(document.getElementById("gridWithViewPortPercentageElement"), "64px", "60px");
testGridTemplatesValues(document.getElementById("gridWithMinMaxElement"), "80px", "300px");
testGridTemplatesValues(document.getElementById("gridWithMinContentElement"), "0px", "0px");
testGridTemplatesValues(document.getElementById("gridWithMinContentWithChildrenElement"), "17px", "11px");
testGridTemplatesValues(document.getElementById("gridWithMaxContentElement"), "0px", "0px");
testGridTemplatesValues(document.getElementById("gridWithMaxContentWithChildrenElement"), "17px", "11px");
testGridTemplatesValues(document.getElementById("gridWithFractionElement"), "800px", "600px");
testGridTemplatesValues(document.getElementById("gridWithCalcElement"), "150px", "75px");
testGridTemplatesValues(document.getElementById("gridWithCalcComplexElement"), "550px", "465px");
testGridTemplatesValues(document.getElementById("gridWithCalcInsideMinMaxElement"), "minmax(10%, 15px)", "minmax(20px, 50%)", "80px", "300px");
testGridTemplatesValues(document.getElementById("gridWithCalcComplexInsideMinMaxElement"), "minmax(10%, 415px)", "minmax(80px, 50%)", "415px", "300px");

debug("");
debug("Test getting wrong values for -webkit-grid-template-columns and -webkit-grid-template-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
testGridTemplatesValues(gridWithFitContentElement, "none", "none");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
testGridTemplatesValues(gridWithFitAvailableElement, "none", "none");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
testGridTemplatesValues(element, "none", "none");

debug("");
debug("Test getting and setting -webkit-grid-template-columns and -webkit-grid-template-rows through JS");
testGridTemplatesSetJSValues("18px", "66px");
testGridTemplatesSetJSValues("55%", "40%", "440px", "240px");
testGridTemplatesSetJSValues("auto", "auto", "0px", "0px");
testGridTemplatesSetJSValues("10vw", "25vh", "80px", "150px");
testGridTemplatesSetJSValues("-webkit-min-content", "-webkit-min-content", "0px", "0px");
testGridTemplatesSetJSValues("-webkit-max-content", "-webkit-max-content", "0px", "0px");

debug("");
debug("Test getting and setting -webkit-grid-template-columns and -webkit-grid-template-rows to minmax() values through JS");
testGridTemplatesSetJSValues("minmax(55%, 45px)", "minmax(30px, 40%)", "440px", "240px");
testGridTemplatesSetJSValues("minmax(22em, 8vh)", "minmax(10vw, 5em)", "220px", "80px");
testGridTemplatesSetJSValues("minmax(-webkit-min-content, 8vh)", "minmax(10vw, -webkit-min-content)", "48px", "80px");
testGridTemplatesSetJSValues("minmax(22em, -webkit-max-content)", "minmax(-webkit-max-content, 5em)", "220px", "50px");
testGridTemplatesSetJSValues("minmax(-webkit-min-content, -webkit-max-content)", "minmax(-webkit-max-content, -webkit-min-content)", "0px", "0px");

// Unit comparison should be case-insensitive.
testGridTemplatesSetJSValues("3600Fr", "154fR", "800px", "600px", "3600fr", "154fr");

// Float values are allowed.
+testGridTemplatesSetJSValues("3.1459fr", "2.718fr", "800px", "600px");

// A leading '+' is allowed.
testGridTemplatesSetJSValues("+3fr", "+4fr", "800px", "600px", "3fr", "4fr");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows to calc() values through JS");
testGridTemplatesSetJSValues("calc(150px)", "calc(75px)", "150px", "75px");
testGridTemplatesSetJSValues("calc(50% - 30px)", "calc(75px + 10%)", "370px", "135px");
testGridTemplatesSetJSValues("minmax(25%, calc(30px))", "minmax(calc(75%), 40px)", "200px", "450px", "minmax(25%, calc(30px))", "minmax(calc(75%), 40px)");
testGridTemplatesSetJSValues("minmax(10%, calc(30px + 10%))", "minmax(calc(25% - 50px), 200px)", "110px", "200px", "minmax(10%, calc(30px + 10%))", "minmax(calc(25% - 50px), 200px)");

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
// Invalid expressions with calc
testGridTemplatesSetBadJSValues("calc(16px 30px)", "calc(25px + auto)");
testGridTemplatesSetBadJSValues("minmax(-webkit-min-content, calc())", "calc(10%(");

debug("");
debug("Test setting grid-template-columns and grid-template-rows back to 'none' through JS");
testGridTemplatesSetJSValues("18px", "66px");
testGridTemplatesSetJSValues("none", "none");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.font = "10px Ahem"; // Used to resolve em font consistently.
    parentElement.style.webkitGridTemplateColumns = "50px 'last'";
    parentElement.style.webkitGridTemplateRows = "'first' 2em";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.display = "-webkit-grid";
    element.style.webkitGridTemplateColumns = "inherit";
    element.style.webkitGridTemplateRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'50px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'first 20px'");

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
    element.style.width = "300px";
    element.style.height = "150px";
    element.style.webkitGridTemplateColumns = "150% 'last'";
    element.style.webkitGridTemplateRows = "'first' 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'450px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'first 150px'");

    element.style.display = "-webkit-grid";
    element.style.webkitGridTemplateColumns = "initial";
    element.style.webkitGridTemplateRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
