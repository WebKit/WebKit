description("This test checks the behaviour of getElementsByTagName in a non-HTML document");

var documentString = "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>" +
                     "<linearGradient/><img/><foreignObject><body xmlns='http://www.w3.org/1999/xhtml'><foo/><FOO/></body></foreignObject>" +
                     "</svg>";

var parser = new DOMParser();
var svgDocument = parser.parseFromString(documentString, "image/svg+xml");
var container = svgDocument.documentElement;

shouldBe("container.getElementsByTagName('linearGradient').length", "1");
shouldBe("container.getElementsByTagName('lineargradient').length", "0");
shouldBe("container.getElementsByTagName('LINEARGRADIENT').length", "0");
shouldBe("container.getElementsByTagName('FOO').length", "1");
shouldBe("container.getElementsByTagName('foo').length", "1");
shouldBe("container.getElementsByTagName('Foo').length", "0");
shouldBe("container.getElementsByTagName('img').length", "1");
shouldBe("container.getElementsByTagName('IMG').length", "0");

shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'linearGradient').length", "1");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'lineargradient').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'LINEARGRADIENT').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'FOO').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'foo').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'Foo').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'img').length", "1");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/2000/svg', 'IMG').length", "0");

shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'linearGradient').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'lineargradient').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'LINEARGRADIENT').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'FOO').length", "1");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'foo').length", "1");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'Foo').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'img').length", "0");
shouldBe("container.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'IMG').length", "0");

var successfullyParsed = true;
