description("This test checks that an SVGFEFloodElement object has no in1 property.");

var feFlood = document.createElementNS("http://www.w3.org/2000/svg", "feFlood");
shouldBe("feFlood.in1", "undefined");

successfullyParsed = true;
