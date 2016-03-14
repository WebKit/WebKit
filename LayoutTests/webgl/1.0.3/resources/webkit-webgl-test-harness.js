"use strict";
(function() {
  var numFailures = 0;

  if (window.testRunner && !window.layoutTestController) {
    window.layoutTestController = window.testRunner;
  }

  if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();

    // Turn off console messages because for the WebGL tests they are
    // GPU capability dependent.
    window.console.log = function() { };
    window.console.error = function() { };
  }


  if (window.internals) {
    window.internals.settings.setWebGLErrorsToConsoleEnabled(false);
  }

  var log = function(msg, color) {
    var div = document.createElement("div");
    div.appendChild(document.createTextNode(msg));
    if (color) {
      div.style.color = color;
    }
    document.getElementById("result").appendChild(div);
  };

  window.webglTestHarness = {
    reportResults: function(url, success, msg) {
      if (!success) {
        ++numFailures;
      }
    },

    notifyFinished: function(url) {
      var iframe = document.getElementById("iframe");
      iframe.innerHTML = "";
      if (numFailures > 0) {
        log("FAIL", "red");
      } else {
        log("PASS", "green");
      }

      if (window.layoutTestController) {
        layoutTestController.notifyDone();
      }
    },
  }
}());

