description('This tests if internals.hasSpellingMarker works for differnt type of elements. '
    + 'This test succeds when there are four elements having "zz ". '
    + 'However, only the last one should not contatin spelling marker.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var testRoot = document.createElement("div");
document.body.insertBefore(testRoot, document.body.firstChild);

function addContainer(markup)
{
    var contatiner = document.createElement("div");
    contatiner.innerHTML = markup;
    testRoot.appendChild(contatiner);

    return contatiner;
}

function typeMisspelling(contatiner)
{
    contatiner.firstChild.focus();
    typeCharacterCommand('z');
    typeCharacterCommand('z');
    typeCharacterCommand(' ');
}

function verifySpellingMarkers(element)
{
    var div = addContainer(element.containerMarkup);
    typeMisspelling(div);

    if (window.internals)
        shouldBecomeEqual("internals.hasSpellingMarker(0, 2)", element.isMisspelled ? "true" : "false", done);
    else
        done();
}

var tests = [
    function() { verifySpellingMarkers({ containerMarkup: "<textarea></textarea>", isMisspelled: true }); },
    function() { verifySpellingMarkers({ containerMarkup: "<input type='text'></input>", isMisspelled: true}); },
    function() { verifySpellingMarkers({ containerMarkup: "<div contenteditable='true'></div>", isMisspelled: true}); },
    function() { verifySpellingMarkers({ containerMarkup: "<div contentEditable='true' spellcheck='false'></div>", isMisspelled: false}); }
];

function done()
{
    var next = tests.shift();
    if (next)
        return setTimeout(next, 0);

    if (window.internals)
        testRoot.style.display = "none";

    finishJSTest();
}
done();

var successfullyParsed = true;
