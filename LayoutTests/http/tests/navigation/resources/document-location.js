function start() {
  if (window.layoutTestController) {
    var n = document.location.search.substring(1);
    if (!n) {
      // page just opened
      layoutTestController.dumpBackForwardList();
      layoutTestController.dumpAsText();
      layoutTestController.waitUntilDone();

      runTest();
    } else {
      // loaded the ?1 navigation
      layoutTestController.notifyDone();
    }
  }
}

function setLocation() {
  document.location = "?1"
}

