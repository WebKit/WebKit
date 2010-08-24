description('Test JS objects as NodeFilters.');

var walker;
var testElement = document.createElement("div");
testElement.id = 'root';
testElement.innerHTML='<div id="A1"><div id="B1"></div><div id="B2"></div></div>';

function filter(node)
{
    debug(" filtering node " + node.id);
    if (node.id == "B1")
        return NodeFilter.FILTER_SKIP;
    return NodeFilter.FILTER_ACCEPT;
}

debug("Testing with raw function filter");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);
shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B2'");

debug("<br>Testing with object filter");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, {
    acceptNode : function(node) {
      debug(" filtering node " + node.id);
      if (node.id == "B1")
          return NodeFilter.FILTER_SKIP;
      return NodeFilter.FILTER_ACCEPT;
    }
  }, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B2'");

debug("<br>Testing with null filter");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, null, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B1'");

debug("<br>Testing with undefined filter");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, undefined, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B1'");

debug("<br>Testing with object lacking acceptNode property");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, {}, false);

shouldThrow("walker.firstChild();");
shouldBe("walker.currentNode.id;", "'root'");
shouldThrow("walker.nextNode();");
shouldBe("walker.currentNode.id;", "'root'");

debug("<br>Testing with object with non-function acceptNode property");
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, { acceptNode: "foo" }, false);

shouldThrow("walker.firstChild();");
shouldBe("walker.currentNode.id;", "'root'");
shouldThrow("walker.nextNode();");
shouldBe("walker.currentNode.id;", "'root'");

debug("<br>Testing with function having acceptNode function");
var filter = function() { return NodeFilter.FILTER_ACCEPT; };
filter.acceptNode = function(node) { return NodeFilter.FILTER_SKIP; };
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B1'");

debug("<br>Testing acceptNode callee");
var filter = {
  acceptNode: function(node) {
    debug('Callee: ' + arguments.callee);
    return NodeFilter.FILTER_ACCEPT;
  }
};
walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");

var successfullyParsed = true;
