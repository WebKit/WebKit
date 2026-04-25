description('Test that setting and getting display: grid and inline-grid works as expected');

debug("Test getting |display| set through CSS");
var gridElement = document.getElementById("gridElement");
var inlineGridElement = document.getElementById("inlineGridElement");
shouldBe("getComputedStyle(gridElement, '').getPropertyValue('display')", "'grid'");
shouldBe("getComputedStyle(inlineGridElement, '').getPropertyValue('display')", "'inline-grid'");

debug("");
debug("Test getting and setting display through JS");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'block'");
element.style.display = "grid";
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'grid'");

element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'block'");
element.style.display = "inline-grid";
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'inline-grid'");
