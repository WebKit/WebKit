/*
 * Original author: Doug Turner <doug.turner@gmail.com>
 *
 * Contributors:
 *     * Antonio Gomes <tonikitoo@webkit.org>
 **/

var gExpectedResults = 0;
var gIndex = 0;
var gClosureCallback = null;
var gFocusedDocument = 0;

function initTest(table, completedCb) {

  gExpectedResults = table;
  gIndex = 0;
  gClosureCallback = completedCb;
  gFocusedDocument = 0;

  prepareMove();
}

function prepareMove()
{
  // When a table has with "DONE", call the completion callback and end the moves.
  if (gExpectedResults[gIndex][0] == "DONE")
    if (gClosureCallback) {
      gClosureCallback();
      return;
    }

  // When a table has an "", just end the moves.
  if (gExpectedResults[gIndex][0] == "")
    return;

  doMove();
}

function doMove()
{
  var direction;

  switch (gExpectedResults[gIndex][0]) {
  case "Up":
    direction = "upArrow";
    break;
  case "Right":
    direction = "rightArrow";
    break;
  case "Down":
    direction = "downArrow";
    break;
  case "Left":
    direction = "leftArrow";
    break;
  case "Space":
    direction = " ";
    break;
  default:
    debug("Action [" + gExpectedResults[gIndex][0] + "] not supported.");
  }

  if (window.testRunner && direction)
    eventSender.keyDown(direction);

  setTimeout(verifyAndAdvance, 15);
}

function verifyAndAdvance()
{
  var direction = gExpectedResults[gIndex][0];
  var expectedID = gExpectedResults[gIndex][1];

  gFocusedDocument = document;
  findFocusedDocument(document);

  var i = 0;
  var mainFrameHasFocus = true;
  for ( ; i < window.frames.length; i++) {
    if (window.frames[i].document.hasFocus()) {
      mainFrameHasFocus = false;
      break;
    }
  }

  shouldBeEqualToString("gFocusedDocument.activeElement.getAttribute(\"id\")", expectedID);

  gIndex++;
  prepareMove();
}

function findFocusedDocument(currentDocument)
{
  var i = 0;
  for ( ; i < currentDocument.defaultView.frames.length; i++) {
    findFocusedDocument(currentDocument.defaultView.frames[i].document);
    if (gFocusedDocument != document)
      return;
  }

  if (currentDocument.hasFocus()) {
    gFocusedDocument = currentDocument;
    return;
  }
}

