description("Test how Node.prefix setter raises NAMESPACE_ERR.");

var href = document.createAttribute("href");
href.value = "#";

// Should not throw.
href.prefix = null;

// Per DOM3 Core spec, setting to empty is implementation dependent.
// Firefox treats empty like null.
href.prefix = "";
shouldBe("href.prefix", "null");

shouldThrow("document.createAttribute('attr').prefix = 'abc'");
shouldThrow("document.createAttributeNS(null, 'attr').prefix = 'abc'");
shouldThrow("document.createElementNS(null, 'attr').prefix = 'abc'");
shouldThrow("document.createAttributeNS('foo', 'bar').prefix = 'xml'");
shouldThrow("document.createElementNS('foo', 'bar').prefix = 'xml'");
shouldThrow("document.createAttribute('attr').prefix = 'xmlns'");
shouldThrow("document.createAttributeNS('foo', 'attr').prefix = 'xmlns'");
shouldThrow("document.createAttribute('xmlns').prefix = 'foo'");
