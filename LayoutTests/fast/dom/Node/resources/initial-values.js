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
shouldBe("attr.attributes", "null");

debug("Attribute creation using createElementNS on an HTML doc:")
attr = document.createAttributeNS("http://www.example.com", "example:foo");
shouldBe("attr.nodeName", "'example:foo'");
shouldBe("attr.name", "'example:foo'");
shouldBe("attr.localName", "'foo'");
shouldBe("attr.namespaceURI", "'http://www.example.com'");
shouldBe("attr.prefix", "'example'");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");
shouldBe("attr.attributes", "null");

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
shouldBe("attr.attributes", "null");

debug("Attribute creation using createElementNS on an XHTML doc:")
attr = xmlDoc.createAttributeNS("http://www.example.com", "example:foo");
shouldBe("attr.nodeName", "'example:foo'");
shouldBe("attr.name", "'example:foo'");
shouldBe("attr.localName", "'foo'");
shouldBe("attr.namespaceURI", "'http://www.example.com'");
shouldBe("attr.prefix", "'example'");
shouldBe("attr.nodeValue", "''");
shouldBe("attr.value", "''");
shouldBe("attr.attributes", "null");

var comment = document.createComment("foo");
shouldBe("comment.nodeName", "'#comment'");
shouldBe("comment.localName", "null");
shouldBe("comment.namespaceURI", "null");
shouldBe("comment.prefix", "null");
shouldBe("comment.nodeValue", "'foo'");
shouldBe("comment.data", "'foo'");
shouldBe("comment.attributes", "null");

shouldThrow("document.createCDATASection('foo')");
var cdata = xmlDoc.createCDATASection("foo");
shouldBe("cdata.nodeName", "'#cdata-section'");
shouldBe("cdata.localName", "null");
shouldBe("cdata.namespaceURI", "null");
shouldBe("cdata.prefix", "null");
shouldBe("cdata.nodeValue", "'foo'");
shouldBe("cdata.data", "'foo'");
shouldBe("cdata.attributes", "null");

var fragment = document.createDocumentFragment();
shouldBe("fragment.nodeName", "'#document-fragment'");
shouldBe("fragment.localName", "null");
shouldBe("fragment.namespaceURI", "null");
shouldBe("fragment.prefix", "null");
shouldBe("fragment.nodeValue", "null");
shouldBe("fragment.attributes", "null");

var doc = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html", null);
shouldBe("doc.nodeName", "'#document'");
shouldBe("doc.localName", "null");
// Spec: http://www.w3.org/TR/DOM-Level-2-Core/core.html#Level-2-Core-DOM-createDocument
// Currently both FF and WebKit return null here, disagreeing with the spec
shouldBe("doc.namespaceURI", "'http://www.w3.org/1999/xhtml'");
shouldBe("doc.prefix", "null");
shouldBe("doc.nodeValue", "null");
shouldBe("doc.attributes", "null");

var doctype = document.implementation.createDocumentType("svg", "-//W3C//DTD SVG 1.1//EN", "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd");
shouldBe("doctype.nodeName", "'svg'");
shouldBe("doctype.name", "'svg'");
shouldBe("doctype.localName", "null");
shouldBe("doctype.namespaceURI", "null");
shouldBe("doctype.prefix", "null");
shouldBe("doctype.nodeValue", "null");
shouldBe("doctype.attributes", "null");

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
shouldBe("element.nodeName", "'html:pre'");
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

// Not possible to create Entity nodes via the DOM, WebKit doesn't create them from parsing

shouldThrow("document.createEntityReference('gt')");
var entityReference = xmlDoc.createEntityReference("gt");
shouldBe("entityReference.nodeName", "'gt'");
shouldBe("entityReference.localName", "null");
shouldBe("entityReference.namespaceURI", "null");
shouldBe("entityReference.prefix", "null");
shouldBe("entityReference.nodeValue", "null");
shouldBe("entityReference.attributes", "null");

// Not possible to create Notation nodes via the DOM, WebKit doesn't create them from parsing

shouldThrow("document.createProcessingInstruction('xml-stylesheet', 'type=\"text/xsl\" href=\"missing.xsl\"')");
var processingInstruction = xmlDoc.createProcessingInstruction('xml-stylesheet', 'type="text/xsl" href="missing.xsl"');
shouldBe("processingInstruction.nodeName", "'xml-stylesheet'");
shouldBe("processingInstruction.localName", "null");
shouldBe("processingInstruction.namespaceURI", "null");
shouldBe("processingInstruction.prefix", "null");
// DOM Core Level 2 and DOM Core Level 3 disagree on ProcessingInstruction.nodeValue
// L2: entire content excluding the target
// L3: same as ProcessingInstruction.data
// We're following Level 3
shouldBe("processingInstruction.nodeValue", "'type=\"text/xsl\" href=\"missing.xsl\"'");
shouldBe("processingInstruction.attributes", "null");
shouldBe("processingInstruction.target", "'xml-stylesheet'");
shouldBe("processingInstruction.data", "'type=\"text/xsl\" href=\"missing.xsl\"'");

var text = document.createTextNode("foo");
shouldBe("text.nodeName", "'#text'");
shouldBe("text.localName", "null");
shouldBe("text.namespaceURI", "null");
shouldBe("text.prefix", "null");
shouldBe("text.nodeValue", "'foo'");
shouldBe("text.data", "'foo'");
shouldBe("text.attributes", "null");

var successfullyParsed = true;
