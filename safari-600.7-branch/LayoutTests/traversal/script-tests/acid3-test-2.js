description("Test of behavior NodeIterator when nodes are removed, from Acid3.");

var iframe = document.createElement("iframe");
iframe.setAttribute("src", "about:blank");
document.body.appendChild(iframe);
var doc = iframe.contentDocument;
for (var i = doc.documentElement.childNodes.length-1; i >= 0; i -= 1)
    doc.documentElement.removeChild(doc.documentElement.childNodes[i]);
doc.documentElement.appendChild(doc.createElement('head'));
doc.documentElement.firstChild.appendChild(doc.createElement('title'));
doc.documentElement.appendChild(doc.createElement('body'));

// test 2: Removing nodes during iteration
var count = 0;
var t1 = doc.body.appendChild(doc.createElement('t1'));
var t2 = doc.body.appendChild(doc.createElement('t2'));
var t3 = doc.body.appendChild(doc.createElement('t3'));
var t4 = doc.body.appendChild(doc.createElement('t4'));
var expect = function(n, node1, node2) {
    count += 1;
    shouldBe("count", "" + n);
    nodea = node1;
    nodeb = node2;
    shouldBe("nodea", "nodeb");
  };
var callCount = 0;
var filterFunctions = [
    function (node) { expect(1, node, doc.body); return true; }, // filter 0
    function (node) { expect(3, node, t1); return true; }, // filter 1
    function (node) { expect(5, node, t2); return true; }, // filter 2
    function (node) { expect(7, node, t3); doc.body.removeChild(t4); return true; }, // filter 3
    function (node) { expect(9, node, t4); return true; }, // filter 4
    function (node) { expect(11, node, t4); doc.body.removeChild(t4); return 2 /* REJECT */; }, // filter 5
    function (node) { expect(12, node, t3); return true; }, // filter 6
    function (node) { expect(14, node, t2); doc.body.removeChild(t2); return true; }, // filter 7
    function (node) { expect(16, node, t1); return true; }, // filter 8
];
var i = doc.createNodeIterator(doc.documentElement.lastChild, 0xFFFFFFFF, function (node) { return filterFunctions[callCount++](node); }, true);
    // * B 1 2 3 4
expect(2, i.nextNode(), doc.body); // filter 0
    // [B] * 1 2 3 4     
expect(4, i.nextNode(), t1); // filter 1
    // B [1] * 2 3 4
expect(6, i.nextNode(), t2); // filter 2
    // B 1 [2] * 3 4
expect(8, i.nextNode(), t3); // filter 3
    // B 1 2 [3] *
doc.body.appendChild(t4);
    // B 1 2 [3] * 4
expect(10, i.nextNode(), t4); // filter 4
    // B 1 2 3 [4] *
expect(13, i.previousNode(), t3); // filters 5, 6
    // B 1 2 3 * (4) // filter 5
    // B 1 2 [3] *   // between 5 and 6
    // B 1 2 * (3)   // filter 6
    // B 1 2 * [3]
expect(15, i.previousNode(), t2); // filter 7
    // B 1 * (2) [3]
    // -- spec says "For instance, if a NodeFilter removes a node
    //    from a document, it can still accept the node, which
    //    means that the node may be returned by the NodeIterator
    //    or TreeWalker even though it is no longer in the subtree
    //    being traversed."
    // -- but it also says "If changes to the iterated list do not
    //    remove the reference node, they do not affect the state
    //    of the NodeIterator."
    // B 1 * [3]
expect(17, i.previousNode(), t1); // filter 8
    // B [1] * 3

document.body.removeChild(iframe);

var successfullyParsed = true;
