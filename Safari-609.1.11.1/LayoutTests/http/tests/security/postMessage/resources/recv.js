function log(msg) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(msg));
    document.getElementById('result').appendChild(div);
}

function recv(e) {
    var msg = 'Received message: data="' + e.data + '" origin="' + e.origin + '"';

    log(msg);

    if (e.data.match(/data="done"/) && window.testRunner)
        testRunner.notifyDone();
}

