description('Tests for speech button click with &lt;input type="text" speech>.');

function onChange() {
    shouldBeEqualToString('document.getElementById("speechInput").value', 'Pictures of the moon');
    finishJSTest();
}

function run() {
  var input = document.createElement('input');
  input.id = 'speechInput';
  input.speech = 'speech';
  input.addEventListener('change', onChange, false);
  document.body.appendChild(input);

  if (window.layoutTestController)
      layoutTestController.setMockSpeechInputResult('Pictures of the moon');

  // Clicking the speech button should fill in mock speech-recognized text.
  if (window.eventSender) {
      var x = input.offsetLeft + input.offsetWidth - 4;
      var y = input.offsetTop + input.offsetHeight / 2;
      eventSender.mouseMoveTo(x, y);
      eventSender.mouseDown();
      eventSender.mouseUp();
  }
}

window.onload = run;
window.jsTestIsAsync = true;
window.successfullyParsed = true;
