description('Test to make sure we return correct text-decoration state.  Note that "text-decoration: none" SHOULD NOT cancel text decorations.')

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function textdecorationState(decoration, content)
{
    testContainer.innerHTML = content;
    var e = document.getElementById('e');
    var s = window.getSelection();
    var r = document.createRange();
    r.setStart(e, 0);
    r.setEnd(e, 1);
    e.focus();
    s.removeAllRanges();
    s.addRange(r);

    return document.queryCommandState(decoration);
}

shouldBe('textdecorationState("underline","<u><b><i><span id=e>hello world</span></i></b></u>")', 'true');
shouldBe('textdecorationState("underline","<b><i><u><span id=e>hello world</span></u></i></b>")', 'true');
shouldBe('textdecorationState("underline","<b><i><span id=e style=\'text-decoration: underline;\'>hello world</span></i></b>")', 'true');
shouldBe('textdecorationState("underline","<span style=\'text-decoration: underline;\'><em id=e>hello world</em></span>")', 'true');
shouldBe('textdecorationState("underline","<u><b><i><span id=e style=\'text-decoration:none\'>hello world</span></i></b></u>")', 'true');

shouldBe('textdecorationState("strikeThrough","<b><i><span id=e>hello world</span></i></b>")', 'false');
shouldBe('textdecorationState("strikeThrough","<s><b><i><span id=e>hello world</span></i></b></s>")', 'true');
shouldBe('textdecorationState("strikeThrough","<b><i><s><span id=e>hello world</span></s></i></b>")', 'true');
shouldBe('textdecorationState("strikeThrough","<b><i><span id=e style=\'text-decoration: line-through;\'>hello world</span></i></b>")', 'true');
shouldBe('textdecorationState("strikeThrough","<span style=\'text-decoration: line-through;\'><em id=e>hello world</em></span>")', 'true');
shouldBe('textdecorationState("strikeThrough","<s><b><i><span id=e style=\'text-decoration:none\'>hello world</span></i></b></s>")', 'true');

document.body.removeChild(testContainer);

var successfullyParsed = true;
