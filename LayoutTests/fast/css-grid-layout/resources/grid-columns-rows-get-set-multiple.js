description('Test that setting and getting grid-columns and grid-rows works as expected');

debug("Test getting |display| set through CSS");
var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-columns')", "'7px 11px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-rows')", "'17px 2px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-columns')", "'53% 99%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-rows')", "'27% 52%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-columns')", "'auto auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-rows')", "'auto auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-columns')", "'100px 120px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-rows')", "'150px 170px'");

var gridWithThreeItems = document.getElementById("gridWithThreeItems");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('-webkit-grid-columns')", "'15px auto 100px'");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('-webkit-grid-rows')", "'120px 18px auto'");

var gridWithPercentAndViewportPercent = document.getElementById("gridWithPercentAndViewportPercent");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('-webkit-grid-columns')", "'50% 120px'");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('-webkit-grid-rows')", "'35% 168px'");

var gridWithFitContentAndFitAvailable = document.getElementById("gridWithFitContentAndFitAvailable");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('-webkit-grid-rows')", "'none'");

var gridWithMinMaxContent = document.getElementById("gridWithMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('-webkit-grid-columns')", "'-webkit-min-content -webkit-max-content'");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('-webkit-grid-rows')", "'-webkit-max-content -webkit-min-content'");

var gridWithMinMaxAndFixed = document.getElementById("gridWithMinMaxAndFixed");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('-webkit-grid-columns')", "'minmax(45px, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('-webkit-grid-rows')", "'120px minmax(35%, 10px)'");

var gridWithMinMaxAndMinMaxContent = document.getElementById("gridWithMinMaxAndMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('-webkit-grid-columns')", "'minmax(-webkit-min-content, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('-webkit-grid-rows')", "'120px minmax(35%, -webkit-max-content)'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test getting and setting display through JS");
element.style.webkitGridColumns = "18px 22px";
element.style.webkitGridRows = "66px 70px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'18px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'66px 70px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "55% 80%";
element.style.webkitGridRows = "40% 63%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'55% 80%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'40% 63%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "auto auto";
element.style.webkitGridRows = "auto auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'auto auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'auto auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridColumns = "auto 16em 22px";
element.style.webkitGridRows = "56% 10em auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'auto 160px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'56% 100px auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridColumns = "16em minmax(16px, 20px)";
element.style.webkitGridRows = "minmax(10%, 15%) auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'160px minmax(16px, 20px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(10%, 15%) auto'");

debug("");
debug("Test getting wrong values set from CSS");
var gridWithNoneAndAuto = document.getElementById("gridWithNoneAndAuto");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('-webkit-grid-rows')", "'none'");

var gridWithNoneAndFixed = document.getElementById("gridWithNoneAndFixed");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test setting and getting wrong values from JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "none auto";
element.style.webkitGridRows = "none auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "none 16em";
element.style.webkitGridRows = "none 56%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "none none";
element.style.webkitGridRows = "none none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "auto none";
element.style.webkitGridRows = "auto none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "auto none 16em";
element.style.webkitGridRows = "auto 18em none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "50% 12vw";
element.style.webkitGridRows = "5% 85vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'50% 96px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'5% 510px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "-webkit-fit-content -webkit-fit-content";
element.style.webkitGridRows = "-webkit-fit-available -webkit-fit-available";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "auto minmax(16px, auto)";
element.style.webkitGridRows = "minmax(auto, 15%) 10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");
