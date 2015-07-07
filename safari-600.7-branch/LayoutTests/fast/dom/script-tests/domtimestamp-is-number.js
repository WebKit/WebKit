description("This tests that DOMTimeStamp is a Number (and not a Date object).");
// See https://bugs.webkit.org/show_bug.cgi?id=49963

var timestamp = null;

function do_check(e) {
  timestamp = e.timeStamp;
  shouldBeFalse("timestamp instanceof Date");
  shouldBeTrue("timestamp == Number(timestamp)");
  finishJSTest();
}
window.jsTestIsAsync = true;