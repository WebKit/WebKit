function clearLog() {
    const logText = document.getElementById('console').firstChild;
    if (logText)
        document.getElementById('console').removeChild(logText);
}
function log(msg) {
    document.getElementById('console').appendChild(document.createTextNode(msg + '\n'));
}
function runTest(expectedSelection, dragOffset) {
    Markup.noAutoDump();
    container.style.fontFamily = 'monospace';
    container.style.fontSize = '10px';
    if (window.testRunner && window.eventSender) {
        testRunner.dumpAsText();

        var x = target.offsetLeft + (target.offsetWidth / 2);
        var y = target.offsetTop + (target.offsetHeight / 2);
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        x = target.offsetLeft + target.offsetWidth + dragOffset;
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseUp();

        checkSelection(expectedSelection);
    } else {
        window.onmouseup = function() {
            window.setTimeout(function() {
                clearLog();
                log("Selected test is: '" + getSelection().toString() + "'\n" + Markup.get(container));
            }, 0);  // Without a timeout the selection is inaccurately printed
        }
    }
}

function checkSelection(expectedSelection) {
    const selectedText = getSelection().toString();
    if (expectedSelection === selectedText) {
        log("SUCCESS: Selected test is '" + selectedText + "' as expected");
    } else {
        log("FAIL: Selection should be '" + expectedSelection + "' but was '" + selectedText + "'");
    }
    log(Markup.get(container));
}
