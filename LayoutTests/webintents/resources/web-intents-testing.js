// Callback function to be used for a successful web intent call.
function onSuccess(data) {
  debug("* got reply: " + data);

  if (window.testRunner) {
    window.testRunner.notifyDone();
  }
}

// Callback function to be used for a failed web intent call.
function onFailure(data) {
  debug("* got failure: " + data);

  if (window.testRunner) {
    window.testRunner.notifyDone();
  }
}

// Launch a web intent call with callbacks.
function startIntentWithCallbacks() {
  navigator.webkitStartActivity(new WebKitIntent("action1", "mime/type1", "test"), onSuccess, onFailure);
  debug("* sent intent");

  if (window.testRunner) {
    window.testRunner.waitUntilDone();
  } else {
    alert('This test needs to run in DRT');
  }
}

// This button press simulator sets the user gesture indicator that an intent
// requires to start.
function simulateButtonPress() {
  var button = document.getElementById("button");
  if (eventSender) {
    eventSender.mouseMoveTo(button.getBoundingClientRect().left + 2, button.getBoundingClientRect().top + 12);
    eventSender.mouseDown();
    eventSender.mouseUp();
  }
}
