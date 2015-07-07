if (window.testRunner) {
    testRunner.dumpAsText()
    testRunner.waitUntilDone()
}

window.addEventListener('message', function (evt) {
    if (evt.data != 'qunit::done')
        return;
    var results = document.getElementsByTagName('iframe')[0].contentDocument.querySelector('#qunit-testresult').textContent;
    var key = 'milliseconds.';
    alert(results.substr(results.indexOf(key) + key.length));
    if (window.testRunner)
        testRunner.notifyDone();
}, false);

document.write('<style>iframe { width: 100%; height: 100%; }</style>');
