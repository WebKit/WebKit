description('Tests if the spellchecker behavior change after the spellcheck attribute changed by the script.');

var root = document.createElement("div");
document.body.appendChild(root);

function childHasSpellingMarker(markup)
{
    root.innerHTML = markup;
    var text = document.getElementById("child").firstChild;
    document.getSelection().setPosition(text, 1);
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');
    var marked = internals.hasSpellingMarker(document, 1, 2);
    root.innerHTML = "";
    return marked;
}

shouldBeFalse("childHasSpellingMarker(\"<div contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>\")");
shouldBeTrue("childHasSpellingMarker(\"<div contentEditable>Foo <span id='child'>[]</span> Baz</div>\")");
shouldBeTrue("childHasSpellingMarker(\"<div contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>\")");
shouldBeFalse("childHasSpellingMarker(\"<div spellcheck='false' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>\")");
shouldBeFalse("childHasSpellingMarker(\"<div spellcheck='false' contentEditable>Foo <span id='child'>[]</span> Baz</div>\")");
shouldBeTrue("childHasSpellingMarker(\"<div spellcheck='false' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>\")");
shouldBeFalse("childHasSpellingMarker(\"<div spellcheck='true' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>\")");
shouldBeTrue("childHasSpellingMarker(\"<div spellcheck='true' contentEditable>Foo <span id='child'>[]</span> Baz</div>\")");
shouldBeTrue("childHasSpellingMarker(\"<div spellcheck='true' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>\")");

var successfullyParsed = true;
