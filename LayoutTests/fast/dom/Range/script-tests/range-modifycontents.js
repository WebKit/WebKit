description(
"Test modification of contents of a range"
);

var r;

// TEST 1: Initial values
debug('<span>Start test 1</span>');
var r = document.createRange();
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "0");
shouldBe("r.startContainer", "document");
shouldBe("r.endContainer", "document");
shouldBeTrue("r.collapsed");
shouldBe("r.commonAncestorContainer", "document");

// TEST 2: Insert a comment into the document
debug('<span>Start test 2</span>');
r.insertNode(document.createComment("test comment"));
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "1");
shouldBe("r.startContainer", "document");
shouldBe("r.endContainer", "document");
shouldBeFalse("r.collapsed");
shouldBe("r.commonAncestorContainer", "document");

// TEST 3: Remove the comment again
debug('<span>Start test 3</span>');
r.deleteContents();
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "0");
shouldBe("r.startContainer", "document");
shouldBe("r.endContainer", "document");
shouldBeTrue("r.collapsed");
shouldBe("r.commonAncestorContainer", "document");

// TEST 4: Insert a document fragment
debug('<span>Start test 4</span>');
var f = document.createDocumentFragment();
var c = document.getElementById("description");
r.setStart(c, 0);
f.appendChild(document.createTextNode("test text node"));
f.appendChild(document.createComment("another text comment"));
f.appendChild(document.createTextNode("another test text node"));
r.insertNode(f);
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "3");
shouldBe("r.startContainer", "c");
shouldBe("r.endContainer", "c");
shouldBeFalse("r.collapsed");
shouldBe("r.commonAncestorContainer", "c");

// TEST 5: Remove the fragment again
debug('<span>Start test 5</span>');
r.deleteContents();
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "0");
shouldBe("r.startContainer", "c");
shouldBe("r.endContainer", "c");
shouldBeTrue("r.collapsed");
shouldBe("r.commonAncestorContainer", "c");

// TEST 6: Insert an empty document fragment
debug('<span>Start test 6</span>');
f = document.createDocumentFragment();
r.insertNode(f);
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "0");
shouldBe("r.startContainer", "c");
shouldBe("r.endContainer", "c");
shouldBeTrue("r.collapsed");
shouldBe("r.commonAncestorContainer", "c");

// TEST 7: Insert a div
debug('<span>Start test 7</span>');
var d = document.createElement("div");
d.appendChild(document.createTextNode("test text node"));
d.appendChild(document.createComment("another text comment"));
d.appendChild(document.createTextNode("another test text node"));
r.insertNode(d);
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "1");
shouldBe("r.startContainer", "c");
shouldBe("r.endContainer", "c");
shouldBeFalse("r.collapsed");
shouldBe("r.commonAncestorContainer", "c");

// TEST 8: Remove the div
debug('<span>Start test 8</span>');
r.deleteContents();
shouldBe("r.startOffset", "0");
shouldBe("r.endOffset", "0");
shouldBe("r.startContainer", "c");
shouldBe("r.endContainer", "c");
shouldBeTrue("r.collapsed");
shouldBe("r.commonAncestorContainer", "c");


var successfullyParsed = true;
