description("Test of exception forwarding for NodeIterator and TreeWalker, derived from an early version of Acid3");

var iteration = 0;
function test(node)
{
    iteration += 1;
    switch (iteration) {
        case 1: case 3: case 4: case 6: case 7: case 8: case 9: case 12: throw "Roses";
        case 2: case 5: case 10: case 11: return true;
        default: throw 0;
    }
}

var i = document.createNodeIterator(document.documentElement, 0xFFFFFFFF, test, true);
shouldThrow("i.nextNode()"); // 1
shouldBe("i.nextNode()", "document.documentElement"); // 2
shouldThrow("i.previousNode()"); // 3
var w = document.createTreeWalker(document.documentElement, 0xFFFFFFFF, test, true);
shouldThrow("w.nextNode()"); // 4
shouldBe("w.nextNode()", "document.documentElement.firstChild"); // 5
shouldThrow("w.previousNode()"); // 6
shouldThrow("w.firstChild()"); // 7
shouldThrow("w.lastChild()"); // 8
shouldThrow("w.nextSibling()"); // 9
shouldBe("w.previousSibling()", "null");
shouldBe("w.nextSibling()", "document.body.previousSibling"); // 10
shouldBe("w.previousSibling()", "document.head"); // 11
shouldBe("iteration", "11");

var successfullyParsed = true;
