"use strict";
(function() {
  var numFailures = 0;
  var resultsList = null;
  var resultNum = 1;

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

  var list = function(msg, color) {
    if (!resultsList) {
      resultsList = document.createElement("ul");
      document.getElementById("result").appendChild(resultsList);
    }

    var item = document.createElement("li");
    item.appendChild(document.createTextNode(msg));
    if (color) {
      item.style.color = color;
    }

    resultsList.appendChild(item);
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
      if (success) {
        list(`[ ${resultNum}: PASS ] ${msg}`, "green");
      } else {
        list(`[ ${resultNum}: FAIL ] ${msg}`, "red");
        ++numFailures;
      }

      ++resultNum;
    },

    notifyFinished: function(url) {
      var iframe = document.getElementById("iframe");
      if (numFailures > 0) {
        log(`[ FAIL ] ${numFailures} failures reported`, "red");
      } else {
        resultsList.innerHTML = "";
        iframe.innerHTML = "";
        log("[ PASS ] All tests passed", "green");
      }

      if (window.layoutTestController) {
        layoutTestController.notifyDone();
      }
    },
  }
}());
