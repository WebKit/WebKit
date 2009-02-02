var _recordedEvents = new Array();
// Number of events we're supposed to receive.
var _expectedEventCount = 0;
// Function invoked when we've received _expectedEventCount events.
var _endFunction;

/* Call this function to record manually transition end events:

Function parameters:
    event [required]: the event passed with "webkitTransitionEnd"

*/
function recordTransitionEndEvent(event)
{
  if (event.type != "webkitTransitionEnd")
    throw("Invalid transition end event!");

  _recordedEvents.push([
    event.propertyName,
    event.target.id,
    Math.round(event.elapsedTime * 1000) / 1000 // round to ms to avoid floating point imprecision
    ]);
  if (_recordedEvents.length == _expectedEventCount)
    _endFunction();
}

/* This is the helper function to run transition end event tests:

Test page requirements:
- The body must contain an empty div with id "result"
- The body must contain a div with with id "container"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining the expected parameter values for the recorded transition end events (see below)
    timeout [required]: 
    callback [optional]: a function to be executed just before the test starts (none by default)

    Each sub-array must contain these items in this order:
    - the name of the CSS property that was transitioning
    - the id of the element on which the CSS property was transitioning
    - the elapsed time in seconds at which the CSS property finished transitioning
    - a boolean indicating if an event listener should be automatically added to the element to record the transition end event or if the script calls recordTransitionEndEvent() directly

*/
function runTransitionTest(expected, timeout, callback)
{
  _expectedEventCount = expected.length;

  if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
  }
  
  function processEndEvents(expected)
  {

    function compareEventInfo(e1, e2)
    {
      // Sort by property name then event target id

      // Index 0 is the property name
      if (e1[0]<e2[0]) return -1;
      if (e1[0]>e2[0]) return +1;

      // Index 1 is the target id
      if (e1[1]<e2[1]) return -1;
      if (e1[1]>e2[1]) return +1;

      return 0;
    }

    function examineResults(results, expected)
    {
      // Sort recorded and expected events arrays so they have the same ordering
      expected.sort(compareEventInfo);
      results.sort(compareEventInfo);

      var result = '<p>';
      for (var i=0; i < results.length && i < expected.length; ++i) {
        var pass = expected[i][0] == results[i][0] && expected[i][1] == results[i][1] && expected[i][2] == results[i][2];

        if (pass)
          result += "PASS --- ";
        else
          result += "FAIL --- ";

        result += "[Expected] Property: " + expected[i][0] + " ";
        result += "Target: " + expected[i][1] + " ";
        result += "Elapsed Time: " + expected[i][2];

        if (!pass) {
          result += " --- ";
          result += "[Received] Property: " + results[i][0] + " ";
          result += "Target: " + results[i][1] + " ";
          result += "Elapsed Time: " + results[i][2];
        }

        result += "<br>";
      }
      result += "</p>";

      if (expected.length > results.length) {
        result += "<p>FAIL - Missing events<br>";
        for (i=results.length; i < expected.length; ++i) {
          result += "[Missing] Property: " + expected[i][0] + " ";
          result += "Target: " + expected[i][1] + " ";
          result += "Elapsed Time: " + expected[i][2] + "<br>";
        }
        result += "</p>";
      } else if (expected.length < results.length) {
        result += "<p>FAIL - Unexpected events<br>";
        for (i=expected.length; i < results.length; ++i) {
          result += "[Unexpected] Property: " + results[i][0] + " ";
          result += "Target: " + results[i][1] + " ";
          result += "Elapsed Time: " + results[i][2] + "<br>";
        }
        result += "</p>";
      }

      return result;
    }

    document.body.removeChild(document.getElementById('container'));
    document.getElementById('result').innerHTML = examineResults(_recordedEvents, expected);

    if (window.layoutTestController)
        layoutTestController.notifyDone();
  }

  function startTest(expected, timeout, callback)
  {
    if (callback)
      callback();
    
    for (var i=0; i < expected.length; ++i) {
      if (expected[i][3]) {
        var box = document.getElementById(expected[i][1]);
        box.addEventListener("webkitTransitionEnd", recordTransitionEndEvent, false);
      }
    }
    
    _endFunction = function() { processEndEvents(expected); };
    window.setTimeout(_endFunction, timeout * 1000);
  }
  
  window.addEventListener('load', function() { startTest(expected, timeout, callback) }, false);
}
