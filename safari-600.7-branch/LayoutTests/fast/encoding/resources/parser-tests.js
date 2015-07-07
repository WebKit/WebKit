document.write('<p>This test suite was converted from http://www.hixie.ch/tests/adhoc/html/parsing/encoding/all.html</p>' +
 '<p>The suite has been broken up to run the tests in smaller batches.</p>' +
 '<ul>Expected failures:' +
   '<li>56, 57, 58, 59 - we do not run scripts during encoding detection phase and parser treats meta inside a script as text, not a tag.</li>' +
   '<li>60 - parser treats meta inside style as text, not a tag.</li>' +
   '<li>97, 99, 102 - we do not run scripts during encoding detection.</li>' +
  '</ul>' +
  '<p>Status: <span id="status">Tests did not run.</span></p>' +
  '<p>Serious failures:</p>' +
  '<ol id="failures">' +
  '</ol>' +
  '<p>(Tests are considered to pass even if they treat Win1254 and ISO-8859-4 as separate.)</p>' +
  '<p><iframe id="test"></iframe></p>');

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var frame = document.getElementById('test');
var current = 0;
var max = 0;

function dotest() {
    var s = current + '';
    while (s.length < 3) s = '0' + s;
    frame.src = 'resources/' + s + '.html';
    window.receivedResults = function () {
        var want = frame.contentWindow.document.getElementById('expected').firstChild.data;
        var have = frame.contentWindow.document.getElementById('encoding').firstChild.data;
        if (have == 'ISO-8859-9')
            have = 'Windows-1254';
        if (want != have) {
            var li = document.createElement('li');
            var a = document.createElement('a');
            a.appendChild(document.createTextNode('test ' + s));
            a.href = s + '.html';
            li.appendChild(a);
            li.appendChild(document.createTextNode(': expected ' + want + '; used ' + have));
            var pre = document.createElement('pre');
            pre.appendChild(document.createTextNode(frame.contentWindow.document.getElementsByTagName('pre')[0].firstChild.data));
            li.appendChild(pre);
            li.value = current;
            document.getElementById('failures').appendChild(li);
        }
        current += 1;
        if (current <= max)
            dotest();
        else {
            frame.parentNode.removeChild(frame);
            document.getElementById('status').innerText = "Tests ran.";
            if (window.testRunner)
                testRunner.notifyDone();
        }
    };
}

function runtests(c, m) {
    current = c;
    max = m;
    dotest();
}

function alert() { }
