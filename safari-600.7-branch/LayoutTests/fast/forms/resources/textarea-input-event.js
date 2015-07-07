description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=15189">https://bugs.webkit.org/show_bug.cgi?id=15189</a><br>');

var handleTextareaOnInputCallCount = 0;
var handleDivOnInputCallCount = 0;
var successfullyParsed = false;

function handleDivOnInput(e) {
    if (e.target !== ta) {
        testFailed('Wrong target');
    } else {
        handleDivOnInputCallCount++;
        shouldBeEqualToString('ta.value', 'x');
    }
}

function handleTextareaOnInput(e) {
    if (e.target !== ta) {
        testFailed('Wrong target');
    } else {
        handleTextareaOnInputCallCount++;
        shouldBeEqualToString('ta.value', 'x');
    }
}

var ta = document.getElementById('ta');
ta.setAttribute('oninput', 'handleTextareaOnInput(event)');

// Change the value before focusing.
ta.value = '';

ta.focus();

if (window.eventSender) {
    eventSender.keyDown('x', []);
    shouldEvaluateTo('handleTextareaOnInputCallCount', 1);
    shouldEvaluateTo('handleDivOnInputCallCount', 1);
    
    // Change programmatically
    ta.value = 'programmatically';
    
    // Should not have triggered the events.
    shouldEvaluateTo('handleTextareaOnInputCallCount', 1);
    shouldEvaluateTo('handleDivOnInputCallCount', 1);
}
