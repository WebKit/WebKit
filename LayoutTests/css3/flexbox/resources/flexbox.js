function insertAfter(nodeToAdd, referenceNode)
{
    if (referenceNode.nextSibling)
        referenceNode.parentNode.insertBefore(nodeToAdd, referenceNode.nextSibling);
    else
        referenceNode.parentNode.appendChild(nodeToAdd);
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

function checkHorizontalBoxen()
{
    var flexboxen = document.getElementsByClassName("horizontal-box");
    Array.prototype.forEach.call(flexboxen, function(flexbox) {
      var failures = [];
      checkExpectedValues(flexbox, failures);

      var child = flexbox.firstChild;
      while (child) {
          checkExpectedValues(child, failures);
          child = child.nextSibling;
      }

      insertAfter(document.createElement("p"), flexbox);
      insertAfter(document.createTextNode(failures.length ? failures.join('') : "PASS"), flexbox);
    });
}
