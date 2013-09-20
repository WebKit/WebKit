
description('For Bug 40092: Spell checking for pasted text.');

jsTestIsAsync = true;

var testRoot = document.createElement("div");
document.body.insertBefore(testRoot, document.body.firstChild);

var testTextArea = document.createElement("textarea");
testRoot.appendChild(testTextArea);

var testInput = document.createElement("input");
testInput.setAttribute("type", "text");
testRoot.appendChild(testInput);

var testEditable = document.createElement("div");
testEditable.setAttribute("contentEditable", "true");
testRoot.appendChild(testEditable);

var testSourcePlain = document.createElement("div");
testSourcePlain.innerHTML = "zz apple";
testRoot.appendChild(testSourcePlain);

var testSourceDecorated = document.createElement("div");
testSourceDecorated.innerHTML = "z<b>z appl</b>e";
testRoot.appendChild(testSourceDecorated);

var testSourceMulti = document.createElement("div");
testSourceMulti.innerHTML = "zz zz zz";
testRoot.appendChild(testSourceMulti);

var sel = window.getSelection();

var tests = [];

function done()
{
    var next = tests.shift();
    if (next)
        return window.setTimeout(next, 0);
    testRoot.style.display = "none";
    finishJSTest();
}

function verifyMarker(node, expectedMarked)
{
    if (node instanceof HTMLInputElement || node instanceof HTMLTextAreaElement) {
        node.focus();
    } else {
        sel.selectAllChildren(node);
    }

    var ok = true;
    for (var i = 0; ok && i < expectedMarked.length; ++i)
        ok = internals.hasSpellingMarker(document, expectedMarked[i][0], expectedMarked[i][1]);

    if (ok) {
        var nodeContent = node instanceof HTMLInputElement || node instanceof HTMLTextAreaElement ? node.value : node.innerHTML;
        testPassed(node.tagName + " has a marker on '" + nodeContent + "'");
    }

    return ok;
}

var destination = null;
var misspelledLocations = null;
function pasteAndVerify(source, dest, expectedMarked)
{
    sel.selectAllChildren(source);
    document.execCommand("Copy");
    if (dest instanceof HTMLInputElement || dest instanceof HTMLTextAreaElement) {
        dest.value = "";
        dest.focus();
    } else {
        dest.innerHTML = "";
        sel.selectAllChildren(dest);
    }
    document.execCommand("Paste");

    if (window.internals) {
        destination = dest;
        misspelledLocations = expectedMarked;
        shouldBecomeEqual('verifyMarker(destination, misspelledLocations)', 'true', done);
    }
};

if (window.internals)
    internals.settings.setAsynchronousSpellCheckingEnabled(true);

tests.push(function() { pasteAndVerify(testSourcePlain, testInput, [[0, 2]]); });
tests.push(function() { pasteAndVerify(testSourceDecorated, testInput, [[0, 2]]); });
tests.push(function() { pasteAndVerify(testSourceMulti, testInput, [[0, 2], [3, 2]]); });

tests.push(function() { pasteAndVerify(testSourcePlain, testTextArea, [[0, 2]]); });
tests.push(function() { pasteAndVerify(testSourceDecorated, testTextArea, [[0, 2]]); });
tests.push(function() { pasteAndVerify(testSourceMulti, testTextArea, [[0, 2], [3, 2]]); });

tests.push(function() { pasteAndVerify(testSourcePlain, testEditable, [[0, 2]]); });
tests.push(function() { pasteAndVerify(testSourceDecorated, testEditable, [[0, 1]]); }); // To check "fo" part of foo.
tests.push(function() { pasteAndVerify(testSourceMulti, testEditable, [[0, 2], [3, 2]]); });
done();

var successfullyParsed = true;
