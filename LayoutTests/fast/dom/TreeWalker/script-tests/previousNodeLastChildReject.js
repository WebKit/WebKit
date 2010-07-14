description('Test for a specific problem with previousNode that failed in older versions of WebKit.');

var testElement = document.createElement("div");
testElement.innerHTML='<div id="A1"><div id="B1"><div id="C1"></div><div id="C2"><div id="D1"></div><div id="D2"></div></div></div><div id="B2"><div id="C3"></div><div id="C4"></div></div></div>';

function filter(node)
{
    if (node.id == "C2")
        return NodeFilter.FILTER_REJECT;
    return NodeFilter.FILTER_ACCEPT;
}

var walker = document.createTreeWalker(testElement, NodeFilter.SHOW_ELEMENT, filter, false);

shouldBe("walker.firstChild(); walker.currentNode.id", "'A1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'C1'");
shouldBe("walker.nextNode(); walker.currentNode.id", "'B2'");
shouldBe("walker.previousNode(); walker.currentNode.id", "'C1'");

var successfullyParsed = true;
