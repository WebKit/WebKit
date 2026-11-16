"use strict";
(function() {
  let resultMessages = [];
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
      resultMessages.push({success, msg});
    },

    notifyFinished: function(url) {
      let numFailures = resultMessages.reduce((v, {success, msg}) => { return v + !success }, 0);
      if (numFailures > 0) {
        resultMessages.forEach(({success, msg}, index) => {
          if (success)
            log(`[ ${index}: PASS ] ${msg}`, "green");
          else
            log(`[ ${index}: FAIL ] ${msg}`, "red");
        })
        log(`[ FAIL ] ${numFailures} failures reported`, "red");
      } else {
        var iframe = document.getElementById("iframe");
        iframe.innerHTML = "";
        log("[ PASS ] All tests passed", "green");
      }

      if (window.layoutTestController) {
        layoutTestController.notifyDone();
      }
    },
  }
}());
