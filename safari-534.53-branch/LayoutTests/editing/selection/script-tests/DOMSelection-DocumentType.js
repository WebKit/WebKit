description("Test to check if setBaseAndExtent guard node with null owner document (Bug 31680)");

var sel = window.getSelection();
var docType = document.implementation.createDocumentType('c');

sel.setBaseAndExtent(docType);
shouldBeNull("sel.anchorNode");

sel.setBaseAndExtent(null, 0, docType, 0);
shouldBeNull("sel.anchorNode");

sel.collapse(docType);
shouldBeNull("sel.anchorNode");

sel.selectAllChildren(docType);
shouldBeNull("sel.anchorNode");

sel.extend(docType, 0);
shouldBeNull("sel.anchorNode");

sel.containsNode(docType);
shouldBeNull("sel.anchorNode");

shouldBeFalse("sel.containsNode(docType)");

var successfullyParsed = true;
