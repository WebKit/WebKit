/*
 * Contributors:
 *     * Antonio Gomes <tonikitoo@webkit.org>
 **/

function check(x, y, hPadding, vPadding, list)
{
  /*
    NodeList nodesFromRect(in long x,
                           in long y,
                           in long hPadding,
                           in long vPadding,
                           in boolean ignoreClipping);
  */
  var nodes = document.nodesFromRect(x, y, hPadding, vPadding, true /* ignoreClipping */);

  if (nodes.length != list.length) {
    testFailed("Different number of nodes for rect" +
              "[" + x + "," + y + "], " +
              "[" + hPadding + "," + vPadding + "]: '" + list.length + "' vs '" + nodes.length + "'");
    return;
  }

  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i] != list[i]) {
      testFailed("Unexpected node #" + i + " for rect " +
                "[" + x + "," + y + "], " +
                "[" + hPadding + "," + vPadding + "]" + " - " + nodes[i]);
      return;
    }
  }

  testPassed("All correct nodes found for rect "  +
           "[" + x + "," + y + "], " +
           "[" + hPadding + "," + vPadding + "]");
}

function getCenterFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt((rect.top + rect.bottom) / 2)};
}

var successfullyParsed = true;
