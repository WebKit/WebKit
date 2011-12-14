description("Test that setSelectedRange resets the selection granularity to CharacterGranularity.");

var textarea = document.createElement('textarea');
textarea.value = "foo bar baz";
document.body.appendChild(textarea);

var rect = textarea.getBoundingClientRect();
var x = rect.left + 10;
var y = rect.top + 10;
if (window.eventSender) {
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

textarea.setSelectionRange(0, 3);
document.execCommand('delete');

// Calling setSelectionRange should reset the granularity to CharacterGranularity, which means
// execCommand('delete') should *not* do a smart delete.
if (textarea.value == " bar baz")
    testPassed("PASSED");
else
    testFailed("FAILED. textarea value should be \" bar baz\" and was \"" + textarea.value + "\"");
    
var successfullyParsed = true;
