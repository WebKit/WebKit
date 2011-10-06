function insertAfter(nodeToAdd, referenceNode)
{
    if (referenceNode.nextSibling)
        referenceNode.parentNode.insertBefore(nodeToAdd, referenceNode.nextSibling);
    else
        referenceNode.parentNode.appendChild(nodeToAdd);
}

function checkSubtreeExpectedValues(parent, failures)
{
    checkExpectedValues(parent, failures);
    Array.prototype.forEach.call(parent.childNodes, function(node) {
        checkSubtreeExpectedValues(node, failures);
    });
}

function checkExpectedValues(node, failures)
{
    var expectedWidth = node.getAttribute && node.getAttribute("data-expected-width");
    if (expectedWidth) {
        if (node.offsetWidth != parseInt(expectedWidth))
            failures.push("Expected " + expectedWidth + " for width, but got " + node.offsetWidth + ". ");
    }

    var expectedHeight = node.getAttribute && node.getAttribute("data-expected-height");
    if (expectedHeight) {
        if (node.offsetHeight != parseInt(expectedHeight))
            failures.push("Expected " + expectedHeight + " for height, but got " + node.offsetHeight + ". ");
    }

    var expectedOffset = node.getAttribute && node.getAttribute("data-offset-x");
    if (expectedOffset) {
        if (node.offsetLeft != parseInt(expectedOffset))
            failures.push("Expected " + expectedOffset + " for offsetLeft, but got " + node.offsetLeft + ". ");
    }

    var expectedOffset = node.getAttribute && node.getAttribute("data-offset-y");
    if (expectedOffset) {
        if (node.offsetTop != parseInt(expectedOffset))
            failures.push("Expected " + expectedOffset + " for offsetTop, but got " + node.offsetTop + ". ");
    }
}

function checkFlexBoxen()
{
    var flexboxen = document.getElementsByClassName("flexbox");
    Array.prototype.forEach.call(flexboxen, function(flexbox) {
        var failures = [];
        checkExpectedValues(flexbox.parentNode, failures);
        checkSubtreeExpectedValues(flexbox, failures);

        var pre = document.createElement('pre');
        pre.appendChild(document.createTextNode(failures.length ? "FAIL:\n" + failures.join('\n') + '\n\n' + flexbox.outerHTML : "PASS"));
        insertAfter(pre, flexbox);
    });
}
