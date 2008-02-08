description("Tests the TreeWalker.")

var subTree = document.createElement('div');
subTree.innerHTML = "<p>Lorem ipsum <span>dolor <b>sit</b> amet</span>, consectetur <i>adipisicing</i> elit, sed do eiusmod <tt>tempor <b><i>incididunt ut</i> labore</b> et dolore magna</tt> aliqua.</p>"
document.body.appendChild(subTree);

var all = function(node) { return true; }
var w = document.createTreeWalker(subTree, 0x01 | 0x08 | 0x10 | 0x20, all, true);

debug("Test that TreeWalker.parent() doesn't set the currentNode to a node not under the root.");
debug("");

shouldBe("w.currentNode", "subTree");
shouldBeNull("w.parentNode()");
shouldBe("w.currentNode", "subTree");

debug("");
debug("Test that we handle setting the currentNode to arbitrary nodes not under the root element.");
debug("");

w.currentNode = document.documentElement;
shouldBeNull("w.parentNode()");
shouldBe("w.currentNode", "document.documentElement");
w.currentNode = document.documentElement;
shouldBe("w.nextNode()", "document.documentElement.firstChild");
shouldBe("w.currentNode", "document.documentElement.firstChild");
w.currentNode = document.documentElement;
shouldBeNull("w.previousNode()");
shouldBe("w.currentNode", "document.documentElement");
w.currentNode = document.documentElement;
shouldBe("w.firstChild()", "document.documentElement.firstChild");
shouldBe("w.currentNode", "document.documentElement.firstChild");
w.currentNode = document.documentElement;
shouldBe("w.lastChild()", "document.documentElement.lastChild");
shouldBe("w.currentNode", "document.documentElement.lastChild");
w.currentNode = document.documentElement;
shouldBeNull("w.nextSibling()");
shouldBe("w.currentNode", "document.documentElement");
w.currentNode = document.documentElement;
shouldBeNull("w.previousSibling()");
shouldBe("w.currentNode", "document.documentElement");

debug("");
debug("Test how we handle the case when the traversed to node within the root, but the currentElement is not.");
debug("");

w.currentNode = subTree.previousSibling;
shouldBe("w.nextNode()", "subTree");
w.currentNode = document.body;
shouldBe("w.lastChild()", "subTree");

// Cleanup
document.body.removeChild(subTree);

var successfullyParsed = true;
