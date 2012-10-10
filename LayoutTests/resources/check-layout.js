if (window.testRunner)
    testRunner.dumpAsText();

(function() {

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

    var expectedWidth = node.getAttribute && node.getAttribute("data-expected-client-width");
    if (expectedWidth) {
        if (node.clientWidth != parseInt(expectedWidth))
            failures.push("Expected " + expectedWidth + " for clientWidth, but got " + node.clientWidth + ". ");
    }

    var expectedHeight = node.getAttribute && node.getAttribute("data-expected-client-height");
    if (expectedHeight) {
        if (node.clientHeight != parseInt(expectedHeight))
            failures.push("Expected " + expectedHeight + " for clientHeight, but got " + node.clientHeight + ". ");
    }

    var expectedOffset = node.getAttribute && node.getAttribute("data-total-x");
    if (expectedOffset) {
        var totalLeft = node.clientLeft + node.offsetLeft;
        if (totalLeft != parseInt(expectedOffset))
            failures.push("Expected " + expectedOffset + " for clientLeft+offsetLeft, but got " + totalLeft + ", clientLeft: " + node.clientLeft + ", offsetLeft: " + node.offsetLeft + ". ");
    }

    var expectedOffset = node.getAttribute && node.getAttribute("data-total-y");
    if (expectedOffset) {
        var totalTop = node.clientTop + node.offsetTop;
        if (totalTop != parseInt(expectedOffset))
            failures.push("Expected " + expectedOffset + " for clientTop+offsetTop, but got " + totalTop + ", clientTop: " + node.clientTop + ", + offsetTop: " + node.offsetTop + ". ");
    }

    var expectedDisplay = node.getAttribute && node.getAttribute("data-expected-display");
    if (expectedDisplay) {
        var actualDisplay = getComputedStyle(node).display;
        if (actualDisplay != expectedDisplay)
            failures.push("Expected " + expectedDisplay + " for display, but got " + actualDisplay + ". ");
    }
}

window.checkLayout = function(selectorList)
{
    if (!selectorList) {
        console.error("You must provide a CSS selector of nodes to check.");
        return;
    }
    var nodes = document.querySelectorAll(selectorList);
    Array.prototype.forEach.call(nodes, function(node) {
        var failures = [];
        checkExpectedValues(node.parentNode, failures);
        checkSubtreeExpectedValues(node, failures);

        var container = node.parentNode.className == 'container' ? node.parentNode : node;

        var pre = document.createElement('pre');
        if (failures.length)
            pre.className = 'FAIL';
        pre.appendChild(document.createTextNode(failures.length ? "FAIL:\n" + failures.join('\n') + '\n\n' + container.outerHTML : "PASS"));
        insertAfter(pre, container);
    });
    var pre = document.querySelector('.FAIL');
    if (pre)
        setTimeout(function() { pre.previousSibling.scrollIntoView(); }, 0);
}

})();
