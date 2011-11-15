description('Test that setting and getting display: -webkit-flexbox and -webkit-inline-flexbox works as expected');

debug("Test getting |display| set through CSS");
var flexboxElement = document.getElementById("flexbox");
shouldBe("getComputedStyle(flexboxElement, '').getPropertyValue('display')", "'-webkit-flexbox'");
var inlineFlexboxElement = document.getElementById("flexboxInline");
shouldBe("getComputedStyle(inlineFlexboxElement, '').getPropertyValue('display')", "'-webkit-inline-flexbox'");

debug("");
debug("Test getting and setting display through JS");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'block'");
element.style.display = "-webkit-flexbox";
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'-webkit-flexbox'");

element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'block'");
element.style.display = "-webkit-inline-flexbox";
shouldBe("getComputedStyle(element, '').getPropertyValue('display')", "'-webkit-inline-flexbox'");
