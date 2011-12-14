description('This tests that Paste commands fires textInput events.');

function toStringLiteral(str)
{
   return "'" + str + "'";
}

var willCancelTextInput = false;
var textInputCount = 0;
var expectedTextEventData = "";

function pastingTextInputHandler(evt)
{
    shouldBe("event.data", toStringLiteral(expectedTextEventData));
    textInputCount++;
    if (willCancelTextInput)
        evt.preventDefault();
}

var testSourceRoot = document.createElement("div");
document.body.appendChild(testSourceRoot);

var testTargetRoot = document.createElement("div");
testTargetRoot.innerHTML += "<input type='text' id='targetInput' value=''>";
testTargetRoot.innerHTML += "<div id='targetEditable' contentEditable>";
testTargetRoot.innerHTML += "<textarea id='targetTextarea' ></textarea>";
document.body.appendChild(testTargetRoot);

testTargetEditable = document.getElementById("targetEditable");
testTargetEditable.addEventListener("textInput", pastingTextInputHandler);
testTargetInput = document.getElementById("targetInput");
testTargetInput.addEventListener("textInput", pastingTextInputHandler);
testTargetTextarea = document.getElementById("targetTextarea");
testTargetTextarea.addEventListener("textInput", pastingTextInputHandler);

var selection = window.getSelection();

function copyPlainText()
{
    testSourceRoot.innerHTML = "<input type='text' value='PlainHello' id='src' />";
    var input = document.getElementById("src");
    input.focus();

    selection.modify("extend", "forward", "line");
    document.execCommand("Copy");
}

function copyRichText()
{
    testSourceRoot.innerHTML = "<div id='src' contentEditable><b>Rich</b>Hello</div>";
    var editable = document.getElementById("src");
    selection.setBaseAndExtent(editable, 0, editable, 0);
    selection.modify("extend", "forward", "line");
    document.execCommand("Copy");
}

function pasteToTargetEditable()
{
    var editable = testTargetEditable;
    editable.innerHTML = "";
    selection.setBaseAndExtent(editable, 0, editable, 0);
    document.execCommand("Paste");

}

function targetEditableShouldHave(value)
{
    shouldBe("testTargetEditable.innerHTML", toStringLiteral(value));
}

function pasteToTargetInput()
{
    var input = testTargetInput;
    input.value = "";
    input.focus();
    document.execCommand("Paste");
}

function targetInputShouldHave(value)
{
    shouldBe("testTargetInput.value", toStringLiteral(value));
}

function pasteToTargetTextarea()
{
    var textarea = testTargetTextarea;
    textarea.value = "";
    textarea.focus();
    document.execCommand("Paste");
}

function targetTextareaShouldHave(value)
{
    shouldBe("testTargetTextarea.value", toStringLiteral(value));
}

var proceedingTestCases = [
    [copyPlainText, pasteToTargetTextarea, targetTextareaShouldHave, "PlainHello", "PlainHello"],
    [copyPlainText, pasteToTargetInput, targetInputShouldHave, "PlainHello", "PlainHello"],
    [copyPlainText, pasteToTargetEditable, targetEditableShouldHave, "PlainHello", ""],
    [copyRichText, pasteToTargetTextarea, targetTextareaShouldHave, "RichHello", "RichHello"],
    [copyRichText, pasteToTargetInput, targetInputShouldHave, "RichHello", "RichHello"],
    [copyRichText, pasteToTargetEditable, targetEditableShouldHave, "<b>Rich</b>Hello", ""],
];

var cancelingTestCases = [
    [copyPlainText, pasteToTargetTextarea, targetTextareaShouldHave, "", "PlainHello"],
    [copyPlainText, pasteToTargetInput, targetInputShouldHave, "", "PlainHello"],
    [copyPlainText, pasteToTargetEditable, targetEditableShouldHave, "", ""],
    [copyRichText, pasteToTargetTextarea, targetTextareaShouldHave, "", "RichHello"],
    [copyRichText, pasteToTargetInput, targetInputShouldHave, "", "RichHello"],
    [copyRichText, pasteToTargetEditable, targetEditableShouldHave, "", ""],
];

function runSingleTest(caseData)
{
    var copy = caseData[0];
    var paste = caseData[1];
    var verifyFunction = caseData[2];
    var verifyParameter = caseData[3];

    expectedTextEventData = caseData[4];

    copy();
    paste();
    verifyFunction(verifyParameter);
}

textInputCount = 0;
willCancelTextInput = false;
for (var i = 0; i < proceedingTestCases.length; ++i)
    runSingleTest(proceedingTestCases[i]);
shouldBe("textInputCount", "proceedingTestCases.length");

textInputCount = 0;
willCancelTextInput = true;
for (var i = 0; i < cancelingTestCases.length; ++i)
    runSingleTest(cancelingTestCases[i]);
shouldBe("textInputCount", "cancelingTestCases.length");

// Hides dataset to make dump clean.
testTargetRoot.style.display = "none";
testSourceRoot.style.display = "none";

var successfullyParsed = true;
