description("Test creation of each type of Node and check intial values")

var xmlDoc = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html", null);

debug("Attribute creation using createElement on an HTML doc:")
var attr = document.createAttribute("foo");
shouldBe("attr.nodeName", "'foo'");
shouldBe("attr.name", "'foo'");
// Spec: http://www.w3.org/TR/DOM-Level-2-Core/core.html#Level-2-Core-DOM-createAttribute
// Both FF and WebKit return "foo" for Attribute.localName, even though the spec says null
shouldBe("attr.localName", "null");
shouldBe("attr.namespaceURI", "null");
shouldBe("attr.prefix", "null");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");

debug("Attribute creation using createElementNS on an HTML doc:")
attr = document.createAttributeNS("http://www.example.com", "example:foo");
shouldBe("attr.nodeName", "'example:foo'");
shouldBe("attr.name", "'example:foo'");
shouldBe("attr.localName", "'foo'");
shouldBe("attr.namespaceURI", "'http://www.example.com'");
shouldBe("attr.prefix", "'example'");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");

debug("Attribute creation using createElement on an XHTML doc:")
attr = xmlDoc.createAttribute("foo");
shouldBe("attr.nodeName", "'foo'");
shouldBe("attr.name", "'foo'");
// Spec: http://www.w3.org/TR/DOM-Level-2-Core/core.html#Level-2-Core-DOM-createAttribute
// Both FF and WebKit return "foo" for Attribute.localName, even though the spec says null
shouldBe("attr.localName", "null");
shouldBe("attr.namespaceURI", "null");
shouldBe("attr.prefix", "null");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");

debug("Attribute creation using createElementNS on an XHTML doc:")
attr = xmlDoc.createAttributeNS("http://www.example.com", "example:foo");
shouldBe("attr.nodeName", "'example:foo'");
shouldBe("attr.name", "'example:foo'");
shouldBe("attr.localName", "'foo'");
shouldBe("attr.namespaceURI", "'http://www.example.com'");
shouldBe("attr.prefix", "'example'");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");

var comment = document.createComment("foo");
shouldBe("comment.nodeName", "'#comment'");
shouldBeUndefined("comment.localName");
shouldBeUndefined("comment.namespaceURI");
shouldBeUndefined("comment.prefix");
shouldBe("comment.nodeValue", "'foo'");
shouldBe("comment.data", "'foo'");

shouldThrow("document.createCDATASection('foo')");
var cdata = xmlDoc.createCDATASection("foo");
shouldBe("cdata.nodeName", "'#cdata-section'");
shouldBeUndefined("cdata.localName");
shouldBeUndefined("cdata.namespaceURI");
shouldBeUndefined("cdata.prefix");
shouldBe("cdata.nodeValue", "'foo'");
shouldBe("cdata.data", "'foo'");

var fragment = document.createDocumentFragment();
shouldBe("fragment.nodeName", "'#document-fragment'");
shouldBeUndefined("fragment.localName");
shouldBeUndefined("fragment.namespaceURI");
shouldBeUndefined("fragment.prefix");
shouldBe("fragment.nodeValue", "null");

var doc = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html", null);
shouldBe("doc.nodeName", "'#document'");
shouldBeUndefined("doc.localName");
shouldBeUndefined("doc.namespaceURI");
shouldBeUndefined("doc.prefix");
shouldBe("doc.nodeValue", "null");

var doctype = document.implementation.createDocumentType("svg", "-//W3C//DTD SVG 1.1//EN", "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd");
shouldBe("doctype.nodeName", "'svg'");
shouldBe("doctype.name", "'svg'");
shouldBeUndefined("doctype.localName");
shouldBeUndefined("doctype.namespaceURI");
shouldBeUndefined("doctype.prefix");
shouldBe("doctype.nodeValue", "null");

debug("Element creation using createElement on an HTML doc:")
var element = document.createElement("pre");
shouldBe("element.nodeName", "'PRE'");
// Spec: http://www.w3.org/TR/DOM-Level-2-Core/core.html#Level-2-Core-DOM-createElement
// FF returns "PRE" for localName, WebKit returns "pre", the spec says we should return null
shouldBe("element.localName", "null");
// FF returns null for namespaceURI, WebKit returns http://www.w3.org/1999/xhtml, the spec says we should return null
shouldBe("element.namespaceURI", "null");
shouldBe("element.prefix", "null");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("Prefixed element creation using createElementNS on an HTML doc:")
element = document.createElementNS("http://www.w3.org/1999/xhtml", "html:pre");
shouldBe("element.nodeName", "'HTML:PRE'");
shouldBe("element.localName", "'pre'");
shouldBe("element.namespaceURI", "'http://www.w3.org/1999/xhtml'");
shouldBe("element.prefix", "'html'");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("SVG Element creation using createElementNS on an HTML doc:")
element = document.createElementNS("http://www.w3.org/2000/svg", "svg");
shouldBe("element.nodeName", "'svg'");
shouldBe("element.localName", "'svg'");
shouldBe("element.namespaceURI", "'http://www.w3.org/2000/svg'");
shouldBe("element.prefix", "null");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("Unknown Element creation using createElementNS on an HTML doc:")
element = document.createElementNS("http://www.webkit.org", "foo:svg");
shouldBe("element.nodeName", "'foo:svg'");
shouldBe("element.localName", "'svg'");
shouldBe("element.namespaceURI", "'http://www.webkit.org'");
shouldBe("element.prefix", "'foo'");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("Element creation using createElementNS on an HTML doc:")
element = document.createElementNS("http://www.w3.org/1999/xhtml", "pre");
// Spec: http://www.w3.org/TR/DOM-Level-3-Core/core.html#ID-104682815 (element.tagName)
// FF and Opera returns "pre" for nodeName as it is an XHTML element, WebKit returns "PRE".
shouldBe("element.nodeName", "'pre'");
shouldBe("element.localName", "'pre'");
shouldBe("element.namespaceURI", "'http://www.w3.org/1999/xhtml'");
shouldBe("element.prefix", "null");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("Element creation using createElement on an XHTML doc:")
element = xmlDoc.createElement("pre");
shouldBe("element.nodeName", "'pre'");
shouldBe("element.localName", "null");
// FF returns null for namespaceURI, WebKit returns http://www.w3.org/1999/xhtml, the spec says we should return null
shouldBe("element.namespaceURI", "null");
shouldBe("element.prefix", "null");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

debug("Element creation using createElementNS on an XHTML doc:")
element = xmlDoc.createElementNS("http://www.w3.org/1999/xhtml", "html:pre");
shouldBe("element.nodeName", "'html:pre'");
shouldBe("element.localName", "'pre'");
shouldBe("element.namespaceURI", "'http://www.w3.org/1999/xhtml'");
shouldBe("element.prefix", "'html'");
shouldBe("element.nodeValue", "null");
shouldBe("element.attributes.toString()", "'[object NamedNodeMap]'");

// Not possible to create Notation nodes via the DOM, WebKit doesn't create them from parsing

var processingInstruction = document.createProcessingInstruction('xml-stylesheet', 'type=\"text/xsl\" href=\"missing.xsl\"');
shouldBe("processingInstruction.nodeName", "'xml-stylesheet'");
shouldBeUndefined("processingInstruction.localName");
shouldBeUndefined("processingInstruction.namespaceURI");
shouldBeUndefined("processingInstruction.prefix");

processingInstruction = xmlDoc.createProcessingInstruction('xml-stylesheet', 'type="text/xsl" href="missing.xsl"');
shouldBe("processingInstruction.nodeName", "'xml-stylesheet'");
shouldBeUndefined("processingInstruction.localName");
shouldBeUndefined("processingInstruction.namespaceURI");
shouldBeUndefined("processingInstruction.prefix");
// DOM Core Level 2 and DOM Core Level 3 disagree on ProcessingInstruction.nodeValue
// L2: entire content excluding the target
// L3: same as ProcessingInstruction.data
// We're following Level 3
shouldBe("processingInstruction.nodeValue", "'type=\"text/xsl\" href=\"missing.xsl\"'");
shouldBe("processingInstruction.target", "'xml-stylesheet'");
shouldBe("processingInstruction.data", "'type=\"text/xsl\" href=\"missing.xsl\"'");

var text = document.createTextNode("foo");
shouldBe("text.nodeName", "'#text'");
shouldBeUndefined("text.localName");
shouldBeUndefined("text.namespaceURI");
shouldBeUndefined("text.prefix");
shouldBe("text.nodeValue", "'foo'");
shouldBe("text.data", "'foo'");
