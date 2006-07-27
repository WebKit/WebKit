description(
"This test checks whether serialized invalid XHTML is valid XML (for bug 9901)."
);

var doc = (new DOMParser()).parseFromString('<input xmlns="http://www.w3.org/1999/xhtml">123</input>', 'text/xml');
var str = (new XMLSerializer()).serializeToString(doc);

shouldBe('doc.firstChild.firstChild.nodeValue', '"123"');
shouldBe('str', "'<input xmlns=\"http://www.w3.org/1999/xhtml\">123</input>'");

var successfullyParsed = true;
