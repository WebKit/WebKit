description("This test checks the implementation of the ElementTraversal API.");

debug('Test with no children');
var noChildren = document.createElement('div');

shouldBe("noChildren.firstElementChild", "null");
shouldBe("noChildren.lastElementChild", "null");
shouldBe("noChildren.previousElementSibling", "null");
shouldBe("noChildren.nextElementSibling", "null");
shouldBe("noChildren.childElementCount", "0");

debug('Test with no element children');
var noElementChildren = document.createElement('div');
noElementChildren.appendChild(document.createComment("comment but not an element"));
noElementChildren.appendChild(document.createTextNode("no elements here"));

shouldBe("noElementChildren.firstElementChild", "null");
shouldBe("noElementChildren.lastElementChild", "null");
shouldBe("noElementChildren.previousElementSibling", "null");
shouldBe("noElementChildren.nextElementSibling", "null");
shouldBe("noElementChildren.childElementCount", "0");

debug('Test with elements');
var children = document.createElement('div');
children.appendChild(document.createComment("first comment"));
var first = document.createElement('p');
children.appendChild(first);
children.appendChild(document.createComment("a comment"));
var last = document.createElement('p');
children.appendChild(last);
children.appendChild(document.createComment("last comment"));

shouldBe("children.firstElementChild", "first");
shouldBe("children.lastElementChild", "last");
shouldBe("first.nextElementSibling", "last");
shouldBe("first.nextElementSibling.nextElementSibling", "null");
shouldBe("last.previousElementSibling", "first");
shouldBe("last.previousElementSibling.previousElementSibling", "null");
shouldBe("children.childElementCount", "2");

var successfullyParsed = true;
