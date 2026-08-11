
if (window.testRunner)
    testRunner.waitUntilDone();

function mousemoved(e)
{
  var resultBox = document.getElementById('mousepos');
  var offsets = 'element id: ' + e.target.id + '<br> clientX: ' + e.clientX + ' clientY: ' + e.clientY + '<br>';
  offsets += 'offsetX: ' + e.offsetX + ' offsetY: ' + e.offsetY;
  resultBox.innerHTML = offsets;
}

function dispatchEvent(clientX, clientY, expectedElementID, expectedOffsetX, expectedOffsetY)
{
  var ev = document.createEvent("MouseEvent");
  ev.initMouseEvent("click", true, true, window, 1, 1, 1, clientX, clientY, false, false, false, false, 0, document);
  ev.expectedElement = expectedElementID;
  ev.expectedOffsetX = expectedOffsetX;
  ev.expectedOffsetY = expectedOffsetY;
  var target = document.elementFromPoint(ev.pageX, ev.pageY);
  target.dispatchEvent(ev);
}

function clicked(event)
{
  var element = event.target;
  
  var result;
  if (element.id == event.expectedElement &&
      event.offsetX == event.expectedOffsetX &&
      event.offsetY == event.expectedOffsetY)
    result = '<span style="color: green">PASS: event at (' + event.clientX + ', ' + event.clientY + ') hit ' + element.id + ' at offset (' + event.offsetX + ', ' + event.offsetY + ')</span>';
  else
    result = '<span style="color: red">FAIL: event at (' + event.clientX + ', ' + event.clientY + ') expected to hit ' + event.expectedElement + ' at (' + event.expectedOffsetX + ', ' + event.expectedOffsetY + ') but hit ' + element.id + ' at (' + event.offsetX + ', ' + event.offsetY + ')</span>';

  log(result);
}

function log(s)
{
  var resultsDiv = document.getElementById('results');
  resultsDiv.innerHTML += s + '<br>';
}

function runTest()
{
    // In non-test mode, show the mouse coords for testing
    if (window.testRunner)
      document.getElementById('mousepos').style.display = 'none';
    else
      document.body.addEventListener('mousemove', mousemoved, false);

    test();
    if (window.testRunner)
        testRunner.notifyDone();
}

window.addEventListener('load', function() {
  window.setTimeout(runTest, 0);
}, false);


