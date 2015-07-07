description("This test checks the SVGAnimatedAngle API - utilizing the orientAngle property of SVGMarkerElement");

var markerElement = document.createElementNS("http://www.w3.org/2000/svg", "marker");

debug("");
debug("Check initial orientAngle value");
shouldBeEqualToString("markerElement.orientAngle.toString()", "[object SVGAnimatedAngle]");
shouldBeEqualToString("markerElement.orientAngle.baseVal.toString()", "[object SVGAngle]");
shouldBe("markerElement.orientAngle.baseVal.value", "0");

debug("");
debug("Check that angles are dynamic, caching value in a local variable and modifying it, should take effect");
var numRef = markerElement.orientAngle.baseVal;
numRef.value = 100;
shouldBe("numRef.value", "100");
shouldBe("markerElement.orientAngle.baseVal.value", "100");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("markerElement.orientAngle.baseVal = -1", "-1");
shouldBeEqualToString("markerElement.orientAngle.baseVal = 'aString'", "aString");
shouldBe("markerElement.orientAngle.baseVal = markerElement", "markerElement");

debug("");
debug("Check that the orientAngle value remained 100, and the baseVal type has not been changed");
shouldBeEqualToString("markerElement.orientAngle.baseVal.toString()", "[object SVGAngle]");
shouldBe("markerElement.orientAngle.baseVal.value", "100");

successfullyParsed = true;
