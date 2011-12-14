description('Test TreeWalker with skipping');

var walker;
var testElement = document.createElement("div");
testElement.innerHTML='<div id="A1">  <div id="B1">  <div id="C1"></div>  </div>  <div id="B2"></div><div id="B3"></div>  </div>';

var skipB1Filter = {
  acceptNode: function(node) {
    if (node.id == 'B1')
      return NodeFilter.FILTER_SKIP;

    return NodeFilter.FILTER_ACCEPT;
  }
}

var skipB2Filter = {
  acceptNode: function(node) {
    if (node.id == 'B2')
      return NodeFilter.FILTER_SKIP;

    return NodeFilter.FILTER_ACCEPT;
  }
}

debug("<br>Testing nextNode")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB1Filter, false);
shouldBe("walker.nextNode(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'C1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B2'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B3'");

debug("<br>Testing firstChild")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB1Filter, false);
shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.firstChild(); walker.currentNode.id", "'C1'");

debug("<br>Testing nextSibling")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB2Filter, false);
shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.firstChild(); walker.currentNode.id", "'B1'");
shouldBe("walker.nextSibling(); walker.currentNode.id", "'B3'");

debug("<br>Testing parentNode")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB1Filter, false);
walker.currentNode = testElement.querySelectorAll('#C1')[0];
shouldBe("walker.parentNode(); walker.currentNode.id", "'A1'");

debug("<br>Testing previousSibling")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB2Filter, false);
walker.currentNode = testElement.querySelectorAll('#B3')[0];
shouldBe("walker.previousSibling(); walker.currentNode.id", "'B1'");

debug("<br>Testing previousNode")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, skipB1Filter, false);
walker.currentNode = testElement.querySelectorAll('#B3')[0];
shouldBe("walker.previousNode(); walker.currentNode.id", "'B2'");
shouldBe("walker.previousNode(); walker.currentNode.id", "'C1'");
shouldBe("walker.previousNode(); walker.currentNode.id", "'A1'");

var successfullyParsed = true;
