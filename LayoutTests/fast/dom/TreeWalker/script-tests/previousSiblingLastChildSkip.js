description('Test for a specific problem with previousSibling that failed in older versions of WebKit.');

var testElement = document.createElement("div");
testElement.innerHTML='<div id="A1"><div id="B1"><div id="C1"></div><div id="C2"><div id="D1"></div><div id="D2"></div></div></div><div id="B2"><div id="C3"></div><div id="C4"></div></div></div>';

function filter(node)
{
    if (node.id == "B1")
        return NodeFilter.FILTER_SKIP;
    return NodeFilter.FILTER_ACCEPT;
}

var walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'C1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'C2'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'D1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'D2'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B2'");
shouldBe("walker.previousSibling(); walker.currentNode.id", "'C2'");

var successfullyParsed = true;
