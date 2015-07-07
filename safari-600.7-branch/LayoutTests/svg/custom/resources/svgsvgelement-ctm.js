if (window.testRunner)
    testRunner.dumpAsText();

var logDiv = document.getElementById('log');
function log(s) {
    logDiv.appendChild(document.createTextNode(s));
    logDiv.appendChild(document.createElement('br'));
}

function stringForMatrix(m) {
    return m + " [(" + m.a.toFixed(2) + ', ' + m.c.toFixed(2) + ')(' + m.b.toFixed(2) + ', ' + m.d.toFixed(2) + ')(' + m.e.toFixed(2) + ', ' + m.f.toFixed(2) + ")]";
}

function printCTMs(name) {
    var element = document.getElementById(name);
    log(name + " CTM: " + stringForMatrix(element.getCTM()));
    log(name + " ScreenCTM: " + stringForMatrix(element.getScreenCTM()));
}

printCTMs("svg1");
printCTMs("svg2");
printCTMs("group");
printCTMs("svg3");

