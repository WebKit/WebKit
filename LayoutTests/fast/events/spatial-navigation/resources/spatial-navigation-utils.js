/*
 * Original author: Doug Turner <doug.turner@gmail.com>
 *
 * Contributors:
 *     * Antonio Gomes <tonikitoo@webkit.org>
 **/

var gExpectedResults;
var gIndex = 0;
var gClosureCallback = null;

function initTest(table, completedCb) {

  gExpectedResults = table;
  gIndex = 0;
  gClosureCallback = completedCb;

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
  var event = document.createEvent('KeyboardEvents');
  event.initKeyboardEvent('keydown', true, true, document.defaultView, gExpectedResults[gIndex][0], 0, false, false, false, false, false);
  document.dispatchEvent(event);

  setTimeout(verifyAndAdvance, 15);
}

function verifyAndAdvance()
{
  var direction = gExpectedResults[gIndex][0];
  var expectedID = gExpectedResults[gIndex][1];

  var i = 0;
  var mainFrameHasFocus = true;
  for(; i < window.frames.length; i++)
    if (window.frames[i].document.hasFocus()) {
      mainFrameHasFocus = false;
      break;
    }

  // This support frames nested in one level deep only.
  if (mainFrameHasFocus)
    shouldBeEqualToString("document.activeElement.getAttribute(\"id\")", expectedID);
  else
    shouldBeEqualToString("document.defaultView.frames[" + i + "].document.activeElement.getAttribute(\"id\")", expectedID);

  gIndex++;
  prepareMove();
}

