
description('For Bug 40092: Spell checking for pasted text.');

testRunner.waitUntilDone();

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

    testRunner.setAsynchronousSpellCheckingEnabled(false);
    testRunner.notifyDone();
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
    return ok;
}

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

    var nretry = 10;
    var nsleep = 1;
    function trial() { 
        var verified = verifyMarker(dest, expectedMarked);
        if (verified) {
            testPassed(dest.tagName + " has a marker on '" + source.innerHTML + "'");
            done();
            return;
        }

        nretry--;
        if (0 == nretry) {
            testFailed(dest.tagName + " should have a marker on '" + source.innerHTML + "'");
            done();
            return;
        }
        
        nsleep *= 2;
        window.setTimeout(trial, nsleep);
    };
    trial();
};

if (window.testRunner)
    testRunner.setAsynchronousSpellCheckingEnabled(true);

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
