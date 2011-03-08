if (window.layoutTestController) {
    layoutTestController.dumpAsText()
    layoutTestController.waitUntilDone()
}

window.addEventListener('message', function (evt) {
    if (evt.data != 'qunit::done')
        return;
    var results = document.getElementsByTagName('iframe')[0].contentDocument.querySelector('#qunit-testresult').textContent;
    var key = 'milliseconds.';
    alert(results.substr(results.indexOf(key) + key.length));
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}, false);

document.write('<style>iframe { width: 100%; height: 100%; }</style>');
