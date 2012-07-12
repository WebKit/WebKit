/*
 * Contributors:
 *     * Antonio Gomes <tonikitoo@webkit.org>
 **/

function check(x, y, topPadding, rightPadding, bottomPadding, leftPadding, list, doc)
{
  if (!window.internals)
    return;

  if (!doc)
    doc = document;

  var nodes = internals.nodesFromRect(doc, x, y, topPadding, rightPadding, bottomPadding, leftPadding, true /* ignoreClipping */, false /* allow shadow content */);
  if (!nodes)
    return;

  if (nodes.length != list.length) {
    testFailed("Different number of nodes for rect" +
              "[" + x + "," + y + "], " +
              "[" + topPadding + "," + rightPadding +
              "," + bottomPadding + "," + leftPadding +
              "]: '" + list.length + "' vs '" + nodes.length + "'");
    return;
  }

  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i] != list[i]) {
      testFailed("Unexpected node #" + i + " for rect " +
                "[" + x + "," + y + "], " +
                "[" + topPadding + "," + rightPadding +
                "," + bottomPadding + "," + leftPadding + "]" + " - " + nodes[i]);
      return;
    }
  }

  testPassed("All correct nodes found for rect");
}

function checkShadowContent(x, y, topPadding, rightPadding, bottomPadding, leftPadding, shadowContent, doc)
{
  if (!window.internals)
    return;

  if (!doc)
    doc = document;

  var nodes = internals.nodesFromRect(doc, x, y, topPadding, rightPadding, bottomPadding, leftPadding, true /* ignoreClipping */, true /* allowShadowContent */);
  if (!nodes)
    return;

  for (var j = 0; j < shadowContent.length; j++) {
    var found = false;
    for (var i = 0; i < nodes.length; i++) {
      if (internals.shadowPseudoId(nodes[i]) == shadowContent[j]) {
          found = true;
          break;
      }
    }

    if (!found) {
      testFailed("Pseudo Id not found: " + shadowContent[j]);
      return;
    }
  }

  testPassed("All correct nodes found for rect");
}

function getCenterFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt((rect.top + rect.bottom) / 2)};
}

function getTopFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt(rect.top)};
}

function getBottomFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt(rect.bottom)};
}
