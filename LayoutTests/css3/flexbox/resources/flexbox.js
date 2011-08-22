function insertAfter(nodeToAdd, referenceNode)
{
    if (referenceNode.nextSibling)
        referenceNode.parentNode.insertBefore(nodeToAdd, referenceNode.nextSibling);
    else
        referenceNode.parentNode.appendChild(nodeToAdd);
}

function checkHorizontalBoxen()
{
    var flexboxen = document.getElementsByClassName("horizontal-box");
    Array.prototype.forEach.call(flexboxen, function(flexbox) {
      var failures = "";
      var child = flexbox.firstChild;
      while (child) {
          var expectedWidth = child.getAttribute && child.getAttribute("data-expected-width");
          if (child.offsetWidth && expectedWidth) {
              if (child.offsetWidth != parseInt(expectedWidth)) {
                  failures += "Expected " + expectedWidth + " for width, but got " + child.offsetWidth + ". ";
              }
          }

          var expectedOffset = child.getAttribute && child.getAttribute("data-offset-x");
          if (child.offsetLeft && expectedOffset) {
              if (child.offsetLeft != parseInt(expectedOffset)) {
                  failures += "Expected " + expectedOffset + " for offsetLeft, but got " + child.offsetLeft + ". ";
              }
          }

          child = child.nextSibling;
      }

      insertAfter(document.createElement("p"), flexbox);
      insertAfter(document.createTextNode(failures ? failures : "PASS"), flexbox);
    });
}
