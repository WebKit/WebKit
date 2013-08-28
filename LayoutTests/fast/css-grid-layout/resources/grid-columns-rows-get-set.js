description('Test that setting and getting grid-definition-columns and grid-definition-rows works as expected');

debug("Test getting -webkit-grid-definition-columns and -webkit-grid-definition-rows set through CSS");
var gridWithNoneElement = document.getElementById("gridWithNoneElement");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'10px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'15px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'53%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'27%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'100px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'150px'");

var gridWithViewPortPercentageElement = document.getElementById("gridWithViewPortPercentageElement");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'64px'");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'60px'");

var gridWithMinMax = document.getElementById("gridWithMinMax");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(10%, 15px)'");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(20px, 50%)'");

var gridWithMinContent = document.getElementById("gridWithMinContent");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('-webkit-grid-definition-columns')", "'-webkit-min-content'");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('-webkit-grid-definition-rows')", "'-webkit-min-content'");

var gridWithMaxContent = document.getElementById("gridWithMaxContent");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('-webkit-grid-definition-columns')", "'-webkit-max-content'");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('-webkit-grid-definition-rows')", "'-webkit-max-content'");

debug("");
debug("Test getting wrong values for -webkit-grid-definition-columns and -webkit-grid-definition-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test getting and setting -webkit-grid-definition-columns and -webkit-grid-definition-rows through JS");
element.style.webkitGridDefinitionColumns = "18px";
element.style.webkitGridDefinitionRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'66px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "55%";
element.style.webkitGridDefinitionRows = "40%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'55%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'40%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "auto";
element.style.webkitGridDefinitionRows = "auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "10vw";
element.style.webkitGridDefinitionRows = "25vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'80px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'150px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "-webkit-min-content";
element.style.webkitGridDefinitionRows = "-webkit-min-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'-webkit-min-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'-webkit-min-content'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "-webkit-max-content";
element.style.webkitGridDefinitionRows = "-webkit-max-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'-webkit-max-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'-webkit-max-content'");

debug("");
debug("Test getting and setting -webkit-grid-definition-columns and -webkit-grid-definition-rows to minmax() values through JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "minmax(55%, 45px)";
element.style.webkitGridDefinitionRows = "minmax(30px, 40%)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(55%, 45px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(30px, 40%)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridDefinitionColumns = "minmax(22em, 8vh)";
element.style.webkitGridDefinitionRows = "minmax(10vw, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(220px, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(80px, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "minmax(-webkit-min-content, 8vh)";
element.style.webkitGridDefinitionRows = "minmax(10vw, -webkit-min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(-webkit-min-content, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(80px, -webkit-min-content)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridDefinitionColumns = "minmax(22em, -webkit-max-content)";
element.style.webkitGridDefinitionRows = "minmax(-webkit-max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(220px, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(-webkit-max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridDefinitionColumns = "minmax(22em, -webkit-max-content)";
element.style.webkitGridDefinitionRows = "minmax(-webkit-max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(220px, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(-webkit-max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridDefinitionColumns = "minmax(-webkit-min-content, -webkit-max-content)";
element.style.webkitGridDefinitionRows = "minmax(-webkit-max-content, -webkit-min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'minmax(-webkit-min-content, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'minmax(-webkit-max-content, -webkit-min-content)'");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to bad minmax value through JS");
element = document.createElement("div");
document.body.appendChild(element);
// No comma.
element.style.webkitGridDefinitionColumns = "minmax(10px 20px)";
// Only 1 argument provided.
element.style.webkitGridDefinitionRows = "minmax(10px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Nested minmax.
element.style.webkitGridDefinitionColumns = "minmax(minmax(10px, 20px), 20px)";
// Only 2 arguments are allowed.
element.style.webkitGridDefinitionRows = "minmax(10px, 20px, 30px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// No breadth value.
element.style.webkitGridDefinitionColumns = "minmax()";
// No comma.
element.style.webkitGridDefinitionRows = "minmax(30px 30% 30em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Auto is not allowed inside minmax.
element.style.webkitGridDefinitionColumns = "minmax(auto, 8vh)";
element.style.webkitGridDefinitionRows = "minmax(10vw, auto)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

// Negative values are not allowed.
element.style.webkitGridDefinitionColumns = "-1px";
element.style.webkitGridDefinitionRows = "-6em";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

element.style.webkitGridDefinitionColumns = "minmax(-1%, 32%)";
element.style.webkitGridDefinitionRows = "minmax(2vw, -6em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows back to 'none' through JS");
element.style.webkitGridDefinitionColumns = "18px";
element.style.webkitGridDefinitionRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'66px'");
element.style.webkitGridDefinitionColumns = "none";
element.style.webkitGridDefinitionRows = "none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", "'none'");
