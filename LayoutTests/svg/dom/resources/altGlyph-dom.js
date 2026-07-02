description(

"This test checks that SVG altGlyph elements create the appropriate DOM object."

);

var altGlyph = document.getElementById("altGlyph");
shouldBe("altGlyph.tagName", '"altGlyph"');
shouldBe("altGlyph.namespaceURI", '"http://www.w3.org/2000/svg"');
successfullyParsed = true;
