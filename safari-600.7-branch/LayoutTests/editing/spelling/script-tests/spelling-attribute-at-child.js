description('Tests if the spellchecker behavior change after the spellcheck attribute changed by the script.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var root = document.createElement("div");
document.body.appendChild(root);

function verifyChildSpellingMarker(markup, expectation)
{
    root.innerHTML = markup;
    var childText = root.firstChild.childNodes[1].firstChild;
    document.getSelection().setPosition(childText, 1);  // [^]
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');
    debug(escapeHTML(root.innerHTML));
    shouldBecomeEqual('internals.hasSpellingMarker(1, 2)', expectation ? "true" : "false", function() {
        root.innerHTML = "";
        debug("");
        done();
    });
}

var tests = [ function() { verifyChildSpellingMarker("<div contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", false); },
              function() { verifyChildSpellingMarker("<div contentEditable>Foo <span id='child'>[]</span> Baz</div>", true); },
              function() { verifyChildSpellingMarker("<div contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", true); },
              function() { verifyChildSpellingMarker("<div spellcheck='false' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", false); },
              function() { verifyChildSpellingMarker("<div spellcheck='false' contentEditable>Foo <span id='child'>[]</span> Baz</div>", false); },
              function() { verifyChildSpellingMarker("<div spellcheck='false' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", true); },
              function() { verifyChildSpellingMarker("<div spellcheck='true' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", false); },
              function() { verifyChildSpellingMarker("<div spellcheck='true' contentEditable>Foo <span id='child'>[]</span> Baz</div>", true); },
              function() { verifyChildSpellingMarker("<div spellcheck='true' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", true); }
];

function done()
{
    var next = tests.shift();
    if (next)
        return window.setTimeout(next, 0);

    finishJSTest();
}
done();

var successfullyParsed = true;
