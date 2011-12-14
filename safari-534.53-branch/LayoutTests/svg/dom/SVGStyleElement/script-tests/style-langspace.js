description("This test checks that SVGStyleElement objects have xmlspace and xmllang properties inherited from SVGLangSpace.");

var style = document.createElementNS("http://www.w3.org/2000/svg", "style");

style.xmlspace = "preserve";
shouldBeEqualToString("style.xmlspace", "preserve");

style.xmllang = "en-AU";
shouldBeEqualToString("style.xmllang", "en-AU");

successfullyParsed = true;
