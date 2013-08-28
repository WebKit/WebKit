description('Test that setting and getting grid-definition-columns and grid-definition-rows works as expected');

debug("Test getting |display| set through CSS");
var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'7px 11px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'17px 2px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'53% 99%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'27% 52%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'auto auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'auto auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'100px 120px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'150px 170px'");

var gridWithThreeItems = document.getElementById("gridWithThreeItems");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('-webkit-grid-definition-columns')", "'15px auto 100px'");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('-webkit-grid-definition-rows')", "'120px 18px auto'");

var gridWithPercentAndViewportPercent = document.getElementById("gridWithPercentAndViewportPercent");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('-webkit-grid-definition-columns')", "'50% 120px'");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('-webkit-grid-definition-rows')", "'35% 168px'");

var gridWithFitContentAndFitAvailable = document.getElementById("gridWithFitContentAndFitAvailable");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

var gridWithMinMaxContent = document.getElementById("gridWithMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('-webkit-grid-definition-columns')", "'-webkit-min-content -webkit-max-content'");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('-webkit-grid-definition-rows')", "'-webkit-max-content -webkit-min-content'");

var gridWithMinMaxAndFixed = document.getElementById("gridWithMinMaxAndFixed");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(45px, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('-webkit-grid-definition-rows')", "'120px minmax(35%, 10px)'");

var gridWithMinMaxAndMinMaxContent = document.getElementById("gridWithMinMaxAndMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(-webkit-min-content, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('-webkit-grid-definition-rows')", "'120px minmax(35%, -webkit-max-content)'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test getting and setting display through JS");
element.style.webkitGridDefinitionColumns = "18px 22px";
element.style.webkitGridDefinitionRows = "66px 70px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'18px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'66px 70px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "55% 80%";
element.style.webkitGridDefinitionRows = "40% 63%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'55% 80%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'40% 63%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "auto auto";
element.style.webkitGridDefinitionRows = "auto auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'auto auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'auto auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridDefinitionColumns = "auto 16em 22px";
element.style.webkitGridDefinitionRows = "56% 10em auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'auto 160px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'56% 100px auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridDefinitionColumns = "16em minmax(16px, 20px)";
element.style.webkitGridDefinitionRows = "minmax(10%, 15%) auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'160px minmax(16px, 20px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(10%, 15%) auto'");

debug("");
debug("Test getting wrong values set from CSS");
var gridWithNoneAndAuto = document.getElementById("gridWithNoneAndAuto");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

var gridWithNoneAndFixed = document.getElementById("gridWithNoneAndFixed");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test setting and getting wrong values from JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "none auto";
element.style.webkitGridDefinitionRows = "none auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "none 16em";
element.style.webkitGridDefinitionRows = "none 56%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "none none";
element.style.webkitGridDefinitionRows = "none none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "auto none";
element.style.webkitGridDefinitionRows = "auto none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "auto none 16em";
element.style.webkitGridDefinitionRows = "auto 18em none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "50% 12vw";
element.style.webkitGridDefinitionRows = "5% 85vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'50% 96px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'5% 510px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "-webkit-fit-content -webkit-fit-content";
element.style.webkitGridDefinitionRows = "-webkit-fit-available -webkit-fit-available";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "auto minmax(16px, auto)";
element.style.webkitGridDefinitionRows = "minmax(auto, 15%) 10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

// Negative values are not allowed.
element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "-10px minmax(16px, 32px)";
element.style.webkitGridDefinitionRows = "minmax(10%, 15%) -10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "10px minmax(16px, -1vw)";
element.style.webkitGridDefinitionRows = "minmax(-1%, 15%) 10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");
