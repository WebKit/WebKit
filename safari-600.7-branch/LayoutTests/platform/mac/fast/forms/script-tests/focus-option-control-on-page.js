description('https://bugs.webkit.org/show_bug.cgi?id=68412 - This test checks to see if option(alt)-tabbing properly focuses form elements that are normally not focused. For testing, the assumption is that by default pressing tab will skip over buttons, and option-tab will include buttons.');

var iteration = 0;
var modifiers;
var result;
function startTest() {
    debug("Pressing tab 4 times:");
    modifiers = undefined;
    testRunner.focusWebView(runKeyPresses);
}

function runKeyPresses() {
    result = '';
    for (var i = 0; i < 4; ++i) {
        result += ' /' + (i + 1) + ':';
        eventSender.keyDown("\t", modifiers);
    }
    iteration++;
    switch (iteration) {
        case 1:
            shouldBe('result', '" /1:focused text field /2: /3:focused text field /4:"');
            debug("Pressing shift-tab 4 times:");
            modifiers = ["shiftKey"];
            testRunner.focusWebView(runKeyPresses);
            break;
        case 2:
            shouldBe('result', '" /1:focused text field /2: /3:focused text field /4:"');
            debug("Pressing option-tab 4 times:");
            modifiers = ["altKey"];
            testRunner.focusWebView(runKeyPresses);
            break;
        case 3:
            shouldBe('result', '" /1:focused first button /2:focused text field /3:focused second button /4:"');
            debug("Pressing shift-option-tab 4 times:");
            modifiers = ["shiftKey", "altKey"];
            testRunner.focusWebView(runKeyPresses);
            break;
        case 4:
            shouldBe('result', '" /1:focused second button /2:focused text field /3:focused first button /4:"');
            testRunner.removeChromeInputField(notifyDone);
            break;
    }
}

function notifyDone() {
    setTimeout(function() { testRunner.notifyDone(); }, 0);
}

function log(val) {
    result += val;
}

/////////////////////////////////
if (window.testRunner && window.eventSender && testRunner.addChromeInputField) {
    window.jsTestIsAsync = true;
    testRunner.addChromeInputField(startTest);
} else
    finishJSTest();

var successfullyParsed = true;
