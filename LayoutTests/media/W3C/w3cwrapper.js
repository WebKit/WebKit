function getVideoURI(dummy) {
  var bool=function(any){return!(any=="no"||!any)};
  return "../../../content/test." + (bool(document.createElement("video").canPlayType('video/ogg; codecs="theora"')) ? "ogv" : "mp4");
}

function getAudioURI(dummy) {
  return "../../../content/test.wav";
}

function test(testFunction) {
  description(document.title);
  if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
  }
  try {
    testFunction();
  }
  catch (e) {
    testFailed('Aborted with exception: ' + e.message);
  }
  debug('<br /><span class="pass">TEST COMPLETE</span>');
  if (window.layoutTestController)
    layoutTestController.notifyDone();
}

function async_test(title, options) {
  description(title);
  if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
  }
  var t = {
    step: function(testFunction) {
      try {
        testFunction();
      }
      catch (e) {
        testFailed('Aborted with exception: ' + e.message);
      }
    },

    done: function() {
      debug('<br /><span class="pass">TEST COMPLETE</span>');
      if (window.layoutTestController) {
        layoutTestController.notifyDone();
      }
    }
  }

  return t;
}

document.write("<p id=description></p><div id=console></div>");
document.write("<scr" + "ipt src='../../../../fast/js/resources/js-test-pre.js'></" + "script>");
document.write("<scr" + "ipt src='../../../../fast/js/resources/js-test-post-function.js'></" + "script>");

assert_equals = function(a, b) { shouldBe('"' + a + '"', '"' + b + '"'); }
assert_true = function(a) { shouldBeTrue("" + a); }
assert_false = function(a) { shouldBeFalse("" + a); }

var successfullyParsed = true;
