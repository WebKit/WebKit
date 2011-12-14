description('Test TreeWalker with skipping');

var walker;
var testElement = document.createElement("div");
testElement.innerHTML='<div id="A1"><div id="B1" class="keep"></div><div id="B2">this text matters</div><div id="B3" class="keep"></div></div>';

var filter = {
  acceptNode: function(node) {
    if (node.className == 'keep')
      return NodeFilter.FILTER_ACCEPT;

    return NodeFilter.FILTER_SKIP;
  }
}

debug("<br>Testing nextSibling")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);
shouldBe("walker.firstChild(); walker.currentNode.id", "'B1'");
shouldBe("walker.nextSibling(); walker.currentNode.id", "'B3'");

debug("<br>Testing previousSibling")
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);
walker.currentNode = testElement.querySelectorAll('#B3')[0];
shouldBe("walker.previousSibling(); walker.currentNode.id", "'B1'");


var successfullyParsed = true;
