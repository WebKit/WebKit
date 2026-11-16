
function runTest() {
    var div = document.getElementsByTagName('div')[0];

    if (!window.testRunner)
        return;

    if (clickOn == 'left')
        x = 5;
    else
        x = div.offsetWidth - 5;

    eventSender.mouseMoveTo(div.offsetLeft + x, div.offsetTop + div.offsetHeight / 2);
    eventSender.leapForward(200);
    eventSender.mouseDown();
    eventSender.leapForward(200);
    eventSender.mouseUp();
    verify();
}

function verify() {
    var div = document.getElementsByTagName('div')[0];

    if (!window.getSelection().isCollapsed)
        return log('FAIL: selection not collapsed');

    var range = window.getSelection().getRangeAt(0);
    if (range.startContainer != div.firstChild)
        return log('FAIL: wrong container');
    if (range.startOffset != expectedOffset)
        return log('FAIL: wrong offset ' + range.startOffset + ', expected ' + expectedOffset);

    return log('PASS');
}

function log(message) {
    document.body.appendChild(document.createTextNode(message));
    document.body.appendChild(document.createElement('br'));
}
