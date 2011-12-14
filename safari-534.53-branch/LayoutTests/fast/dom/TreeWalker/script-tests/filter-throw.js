description('Test JS objects as NodeFilters.');

var walker;
var testElement = document.createElement("div");
testElement.id = 'root';
testElement.innerHTML='<div id="A1"><div id="B1"></div><div id="B2"></div></div>';
debug("Testing with object filter");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, function(node) {
  throw('filter exception');
  return NodeFilter.FILTER_ACCEPT;
}, false);

debug("Test with filter function");
shouldThrow("walker.firstChild();");
shouldBe("walker.currentNode.id", "'root'")
shouldThrow("walker.nextNode();walker.currentNode.id");
shouldBe("walker.currentNode.id", "'root'")

walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, {
    acceptNode : function(node) {
      throw('filter exception');
      return NodeFilter.FILTER_ACCEPT;
    }
  }, false);

debug("<br>Test with filter object");
shouldThrow("walker.firstChild();");
shouldBe("walker.currentNode.id", "'root'")
shouldThrow("walker.nextNode();walker.currentNode.id");
shouldBe("walker.currentNode.id", "'root'")

var successfullyParsed = true;
