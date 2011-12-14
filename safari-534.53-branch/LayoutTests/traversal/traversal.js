//-------------------------------------------------------------------------------------------------------
// Java script library to run traversal layout tests

function dumpNodeIterator(it, expectedResult) {
    var string = '<p><b>Expect ' + expectedResult + '</b>: ';
    var checkIt = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT, testNodeFiter, false);
    var printedPointer = false;
    while (1) {
        var node = checkIt.nextNode();
        if (!node)
            break;
        if (!printedPointer && (it.referenceNode == node || it.referenceNode == undefined)) {
            printedPointer = true;
            var s = it.referenceNode == undefined ? node.id + ' ' : '[' + node.id + '] ';
            if (it.pointerBeforeReferenceNode)
                string += "* " + s;
            else   
                string += s + "* ";
        }
        else {
            string += node.id + " ";
        }
    }
    return string;
}

//-------------------------------------------------------------------------------------------------------

function nodeDepth(node) {
    var depth = 0;
    while ((node = node.parentNode))
        depth++;
    return depth;
}

//-------------------------------------------------------------------------------------------------------

function dumpTreeWalker(tw, root, start, backwards) {
    var string = '<p><b>Tree Walker: </b><br>';
    if (root == undefined)
        root = tw.currentNode;
    var rootDepth = nodeDepth(root);
    if (start == undefined)
        start = root;
    tw.currentNode = start;
    string +=  start.id + '<br>';   
    while (1) {
        var current = tw.currentNode;
        if (backwards == undefined)
            tw.nextNode();
        else
            backwards ? tw.previousNode() : tw.nextNode();
        var n = tw.currentNode;
        if (current == n)
            break;
        var depth = nodeDepth(n) - rootDepth;
        for (i = 0; i < depth; i++)
            string += '  ';   
        string +=  n.id + '<br>';   
    }
    return string;
}

//-------------------------------------------------------------------------------------------------------

