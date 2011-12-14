description("Test the isEqualNode API.");

debug("Test isEqualNode for DocumentType nodes.");
var docTypeAllSet = document.implementation.createDocumentType('html', '-//W3C//DTD XHTML 1.0 Strict//EN', 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd');
var docTypeAllSet2 = document.implementation.createDocumentType('html', '-//W3C//DTD XHTML 1.0 Strict//EN', 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd');
var docTypeDifferentPublicID = document.implementation.createDocumentType('html', 'foo', 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd');
var docTypeDifferentSystemID = document.implementation.createDocumentType('html', '-//W3C//DTD XHTML 1.0 Strict//EN', 'bar');

shouldBeTrue("docTypeAllSet.isEqualNode(docTypeAllSet2)");
shouldBeFalse("docTypeAllSet.isEqualNode(docTypeDifferentPublicID)");
shouldBeFalse("docTypeAllSet.isEqualNode(docTypeDifferentSystemID)");

var successfullyParsed = true;
