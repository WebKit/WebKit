
description('For Bug 41423: Spelling marker should remain after hitting a backspace key.');

var testRoot = document.createElement("div");
document.body.insertBefore(testRoot, document.body.firstChild);

function setup(targetName)
{
    testRoot.innerHTML = "<div id='" + targetName + "' contentEditable><div>OK</div><div>OK zz OK</div></div>";
    document.getElementById(targetName).focus();
    return document.getSelection();
}

function firstLineText()
{
    return testRoot.firstChild.firstChild.innerText.trim();
}

function testWithDelete()
{
    window.sel = setup("target1");

    sel.modify("move", "forward", "line");
    for (var i = 0; i < 3; i++) // 3 for ["OK, "zz", "OK"].length
        sel.modify("move", "forward", "word");

    shouldBe("firstLineText()", "'OK'");
    shouldBe("sel.anchorNode.data", "'OK zz OK'");
    shouldBe("textInputController.hasSpellingMarker(3, 2)", "1");

    sel.modify("move", "left", "lineboundary");
    document.execCommand("Delete", false);
    sel.modify("move", "right", "line"); // Moves to the line ending to focus the "OK zz OK" text.

    shouldBe("sel.anchorNode.data", "'OK zz OK'");
    shouldBe("firstLineText()", "'OKOK zz OK'");
    shouldBe("textInputController.hasSpellingMarker(3, 2)", "1");
}

function testWithForwardDelete()
{
    window.sel = setup("target1");

    sel.modify("move", "forward", "line");
    for (var i = 0; i < 3; i++) // 3 for ["OK, "zz", "OK"].length
        sel.modify("move", "forward", "word");

    shouldBe("firstLineText()", "'OK'");
    shouldBe("sel.anchorNode.data", "'OK zz OK'");
    shouldBe("textInputController.hasSpellingMarker(3, 2)", "1");

    sel.modify("move", "left", "line");
    document.execCommand("ForwardDelete", false);
    sel.modify("move", "right", "line"); // Moves to the line ending to focus the "OK zz OK" text.

    shouldBe("firstLineText()", "'OKOK zz OK'");
    shouldBe("sel.anchorNode.data", "'OK zz OK'");
    shouldBe("textInputController.hasSpellingMarker(3, 2)", "1");
}

testWithDelete();
testWithForwardDelete();
testRoot.style.display = "none";

var successfullyParsed = true;
