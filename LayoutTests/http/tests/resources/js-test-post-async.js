// DEPRECATED: Do not include this file in new tests. Use js-test.js instead,
// which combines the functionality of js-test-pre.js, js-test-post.js, and
// js-test-post-async.js.

if (!errorMessage)
    successfullyParsed = true;
shouldBeTrue("successfullyParsed");
debug('<br /><span class="pass">TEST COMPLETE</span>');

if (window.testRunner)
    testRunner.notifyDone();
