description('Test that setting and getting grid-columns and grid-rows works as expected');

debug("Test getting -webkit-grid-columns and -webkit-grid-rows set through CSS");
var gridWithNoneElement = document.getElementById("gridWithNoneElement");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('-webkit-grid-rows')", "'none'");

var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-columns')", "'10px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('-webkit-grid-rows')", "'15px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-columns')", "'53%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('-webkit-grid-rows')", "'27%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-columns')", "'auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('-webkit-grid-rows')", "'auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-columns')", "'100px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('-webkit-grid-rows')", "'150px'");

var gridWithViewPortPercentageElement = document.getElementById("gridWithViewPortPercentageElement");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('-webkit-grid-columns')", "'64px'");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('-webkit-grid-rows')", "'60px'");

var gridWithMinMax = document.getElementById("gridWithMinMax");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('-webkit-grid-columns')", "'minmax(10%, 15px)'");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('-webkit-grid-rows')", "'minmax(20px, 50%)'");

var gridWithMinContent = document.getElementById("gridWithMinContent");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('-webkit-grid-columns')", "'-webkit-min-content'");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('-webkit-grid-rows')", "'-webkit-min-content'");

var gridWithMaxContent = document.getElementById("gridWithMaxContent");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('-webkit-grid-columns')", "'-webkit-max-content'");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('-webkit-grid-rows')", "'-webkit-max-content'");

debug("");
debug("Test getting wrong values for -webkit-grid-columns and -webkit-grid-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('-webkit-grid-rows')", "'none'");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test getting and setting -webkit-grid-columns and -webkit-grid-rows through JS");
element.style.webkitGridColumns = "18px";
element.style.webkitGridRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'66px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "55%";
element.style.webkitGridRows = "40%";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'55%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'40%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "auto";
element.style.webkitGridRows = "auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "10vw";
element.style.webkitGridRows = "25vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'80px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'150px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "-webkit-min-content";
element.style.webkitGridRows = "-webkit-min-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'-webkit-min-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'-webkit-min-content'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "-webkit-max-content";
element.style.webkitGridRows = "-webkit-max-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'-webkit-max-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'-webkit-max-content'");

debug("");
debug("Test getting and setting -webkit-grid-columns and -webkit-grid-rows to minmax() values through JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "minmax(55%, 45px)";
element.style.webkitGridRows = "minmax(30px, 40%)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(55%, 45px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(30px, 40%)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridColumns = "minmax(22em, 8vh)";
element.style.webkitGridRows = "minmax(10vw, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(220px, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(80px, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "minmax(-webkit-min-content, 8vh)";
element.style.webkitGridRows = "minmax(10vw, -webkit-min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(-webkit-min-content, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(80px, -webkit-min-content)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridColumns = "minmax(22em, -webkit-max-content)";
element.style.webkitGridRows = "minmax(-webkit-max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(220px, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(-webkit-max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.webkitGridColumns = "minmax(22em, -webkit-max-content)";
element.style.webkitGridRows = "minmax(-webkit-max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(220px, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(-webkit-max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.webkitGridColumns = "minmax(-webkit-min-content, -webkit-max-content)";
element.style.webkitGridRows = "minmax(-webkit-max-content, -webkit-min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'minmax(-webkit-min-content, -webkit-max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'minmax(-webkit-max-content, -webkit-min-content)'");

debug("");
debug("Test setting grid-columns and grid-rows to bad minmax value through JS");
element = document.createElement("div");
document.body.appendChild(element);
// No comma.
element.style.webkitGridColumns = "minmax(10px 20px)";
// Only 1 argument provided.
element.style.webkitGridRows = "minmax(10px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Nested minmax.
element.style.webkitGridColumns = "minmax(minmax(10px, 20px), 20px)";
// Only 2 arguments are allowed.
element.style.webkitGridRows = "minmax(10px, 20px, 30px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// No breadth value.
element.style.webkitGridColumns = "minmax()";
// No comma.
element.style.webkitGridRows = "minmax(30px 30% 30em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Auto is not allowed inside minmax.
element.style.webkitGridColumns = "minmax(auto, 8vh)";
element.style.webkitGridRows = "minmax(10vw, auto)";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test setting grid-columns and grid-rows back to 'none' through JS");
element.style.webkitGridColumns = "18px";
element.style.webkitGridRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'66px'");
element.style.webkitGridColumns = "none";
element.style.webkitGridRows = "none";
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");
