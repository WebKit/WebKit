description('This tests internals.hasSpellingMarker works for differnt type of elements.');

var parent = document.createElement("div");
document.body.appendChild(parent);

function hasMarked(markup)
{
    parent.innerHTML = markup;
    document.getElementById('test').focus();
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');

    return internals.hasSpellingMarker(document, 0, 2);
}

shouldBeTrue("hasMarked(\"<textarea id='test' cols='80' rows='10'></textarea>\");");
shouldBeTrue("hasMarked(\"<input id='test' type='text'>\");");
shouldBeTrue("hasMarked(\"<div id='test' contenteditable='true'></div>\");");
shouldBeFalse("hasMarked(\"<div id='test' contentEditable='true' spellcheck='false'></div>\");");
parent.innerHTML = "";

var successfullyParsed = true;
