//
// REGRESSION (r154769): Wrong <title> taken as a tooltip for SVG element.
// https://bugs.webkit.org/show_bug.cgi?id=139690.
//
// This code mimics the C++ code for calculating the tooltip text for an SVG element. 
// It also verifies the tooltip text for an SVG element and all its descendants. If
// mismatches are found, it logs them by calling the function "log()" which should
// be defined by the test.
//

// Returns true if an element can return a title or not
function canReturnTitle(element)
{
  if (!element || element.tagName == "svg" && element.ownerDocument instanceof SVGDocument)
      return false;

  return true;
}

// Searches the tree elements upward till it finds an SVGTitleElement child
function titleElementFromElementUpward(element)
{
  for (; canReturnTitle(element); element = element.parentElement) {

      var children = element.childNodes;

      // Search immediate children and if any of them is a title element return it
      for (var i = 0; i < children.length; i++) {
        if (children[i].tagName == "title")
          return children[i];
      }
  }
        
  return null;
}

// Returns the string of the SVGTitleElement child if it exists in the element's
// children list or in its parent's children list.
function titleFromElementUpward(element)
{
    var titleElement = titleElementFromElementUpward(element);
    return !titleElement ? "" : titleElement.textContent;
}

// Verifies the tooltip text of an SVG element 
function verifyElementToolTip(element)
{
  var actual = internals.toolTipFromElement(element);
  var expected = titleFromElementUpward(element);

  if (actual == expected)
    return true;
    
  // If the C++ code expected tooltip is different from the calculated tooltip, log
  // them to the actual result file.
  log(element.tagName + ":");
  log("\tid:\t\t" + element.id);
  log("\tactual:\t\t'" + actual + "'");
  log("\texpected:\t'" + expected + "'\n");
  return false;
}

// Verifies the tooltip text for an SVG element and all its descendants
function verifyElementTreeToolTips(element)
{
  var children = element.childNodes;
  var result = verifyElementToolTip(element);

  // Verify the tooltips of the children elements
  for (var i = 0; i < children.length; i++) {
    if (children[i].nodeType != document.ELEMENT_NODE)
      continue;
      
    if (children[i].tagName == "defs" || children[i].tagName == "title")
      continue;
      
    result &= verifyElementTreeToolTips(children[i]);
  }
  
  return result;
}
