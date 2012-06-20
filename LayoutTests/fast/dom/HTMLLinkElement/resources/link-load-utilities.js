if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
}

function shouldComputedColorOfElementBeEqualToRGBString(element, expectedColor)
{
  var elementName = "#" + element.id || element.tagName;
  var actualColor = window.getComputedStyle(element, null).color;
  if (actualColor === expectedColor)
    log("PASS " + elementName + " color was " + expectedColor + ".");
  else
    log("FAIL " + elementName + " color should be " + expectedColor + ". Was " + actualColor + ".");
}

function createLinkElementWithStylesheet(stylesheetURL)
{
  var link = document.createElement("link");
  link.rel = "stylesheet";
  link.href = stylesheetURL;
  return link;
}

function createStyleElementWithString(stylesheetData)
{
  var style = document.createElement("style");
  style.textContent = stylesheetData;
  return style;
}

function log(message)
{
  document.getElementById("console").appendChild(document.createTextNode(message + "\n"));
}

function testPassed(message)
{
  log("PASS " + message);
}

function testFailed(message)
{
  log("FAIL " + message);
}

function testPassedAndNotifyDone(message)
{
  testPassed(message);
  testFinished();
}

function testFailedAndNotifyDone(message)
{
  testFailed(message);
  testFinished();
}

function testFinished()
{
  if (window.testRunner)
    testRunner.notifyDone();
}
