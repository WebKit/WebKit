function start() {
  if (window.testRunner) {
    var n = document.location.search.substring(1);
    if (!n) {
      // page just opened
      testRunner.dumpBackForwardList();
      testRunner.dumpAsText();
      testRunner.waitUntilDone();

      // Location changes need to happen outside the onload handler to generate history entries.
      setTimeout(runTest, 0);
    } else {
      // loaded the ?1 navigation
      testRunner.notifyDone();
    }
  }
}

function setLocation() {
  document.location = "?1"
}

