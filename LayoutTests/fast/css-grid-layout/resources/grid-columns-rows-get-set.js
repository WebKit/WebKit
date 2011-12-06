description('Test that setting and getting grid-columns and grid-rows works as expected');

debug("Test getting |display| set through CSS");
var gridElement = document.getElementById("gridElement");
shouldBe("getComputedStyle(gridElement, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(gridElement, '').getPropertyValue('-webkit-grid-rows')", "'none'");

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

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('-webkit-grid-rows')", "'none'");

debug("");
debug("Test getting and setting display through JS");
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
