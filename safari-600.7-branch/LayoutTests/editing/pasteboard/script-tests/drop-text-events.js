description('This tests that Drag drop fires textInput events.');

function toStringLiteral(str)
{
   return "'" + str + "'";
}

var willCancelTextInput = false;
var textInputCount = 0;
var expectedTextEventData = "";
var actualTextEventData = null;

function droppingTextInputHandler(evt)
{
    actualTextEventData = evt.data;
    textInputCount++;
    if (willCancelTextInput)
        evt.preventDefault();
}

var testSourceRoot = document.createElement("div");
document.body.appendChild(testSourceRoot);

var testTargetRoot = document.createElement("div");
testTargetRoot.innerHTML += "<input type='text' id='targetInput' value=''>";
testTargetRoot.innerHTML += "<div><span id='targetEditable' contentEditable></span></div>";
testTargetRoot.innerHTML += "<textarea id='targetTextarea' ></textarea>";
document.body.appendChild(testTargetRoot);

testTargetEditable = document.getElementById("targetEditable");
testTargetEditable.addEventListener("textInput", droppingTextInputHandler);
testTargetInput = document.getElementById("targetInput");
testTargetInput.addEventListener("textInput", droppingTextInputHandler);
testTargetTextarea = document.getElementById("targetTextarea");
testTargetTextarea.addEventListener("textInput", droppingTextInputHandler);

var selection = window.getSelection();

function clearTargets()
{
    testTargetEditable.innerHTML = "placeholder"; // give some text to have an area to drop
    testTargetInput.value = "";
    testTargetTextarea.value = "";
    actualTextEventData = null;
}

function dragFrom(element)
{
    var x = element.offsetLeft + element.offsetWidth / 2;
    var y = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();

    // Makes drag happen
    var meaninglessDelta = 10; 
    eventSender.leapForward(500);
    eventSender.mouseMoveTo(x + meaninglessDelta , y + meaninglessDelta);
}

function dropTo(element)
{
    var x = element.offsetLeft + element.offsetWidth / 2;
    var y = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseUp();
}

function dragPlainText()
{
    testSourceRoot.innerHTML = "<input type='text' value='PlainHello' id='src' />";
    var input = document.getElementById("src");
    input.focus();
    selection.modify("extend", "forward", "line");
    dragFrom(input);
}

function dragRichText()
{
    testSourceRoot.innerHTML = "<div id='src' contentEditable><b>Rich</b>Hello</div>";
    var editable = document.getElementById("src");
    selection.setBaseAndExtent(editable, 0, editable, 0);
    selection.modify("extend", "forward", "line");
    dragFrom(editable);
}

function dropToTargetEditable()
{
    var editable = testTargetEditable;
    dropTo(editable);
}

function targetEditableShouldHave(value)
{
    shouldBeTrue("0 <= testTargetEditable.innerHTML.indexOf(" + toStringLiteral(value) + ")");
}

function targetEditableShouldStayAsIs()
{
    shouldBe("testTargetEditable.innerHTML", toStringLiteral("placeholder"));
}

function dropToTargetInput()
{
    var input = testTargetInput;
    dropTo(input);
}

function targetInputShouldHave(value)
{
    shouldBe("testTargetInput.value", toStringLiteral(value));
}

function dropToTargetTextarea()
{
    var textarea = testTargetTextarea;
    dropTo(textarea);
}

function targetTextareaShouldHave(value)
{
    shouldBe("testTargetTextarea.value", toStringLiteral(value));
}

var proceedingTestCases = [
    [dragPlainText, dropToTargetTextarea, targetTextareaShouldHave, "PlainHello", "PlainHello"],
    [dragPlainText, dropToTargetInput, targetInputShouldHave, "PlainHello", "PlainHello"],
    [dragPlainText, dropToTargetEditable, targetEditableShouldHave, "PlainHello", ""],
    [dragRichText, dropToTargetTextarea, targetTextareaShouldHave, "RichHello", "RichHello"],
    [dragRichText, dropToTargetInput, targetInputShouldHave, "RichHello", "RichHello"],
    [dragRichText, dropToTargetEditable, targetEditableShouldHave, "<b>Rich</b>Hello", ""],
];

var cancelingTestCases = [
    [dragPlainText, dropToTargetTextarea, targetTextareaShouldHave, "", "PlainHello"],
    [dragPlainText, dropToTargetInput, targetInputShouldHave, "", "PlainHello"],
    [dragPlainText, dropToTargetEditable, targetEditableShouldStayAsIs, "", ""],
    [dragRichText, dropToTargetTextarea, targetTextareaShouldHave, "", "RichHello"],
    [dragRichText, dropToTargetInput, targetInputShouldHave, "", "RichHello"],
    [dragRichText, dropToTargetEditable, targetEditableShouldStayAsIs, "", ""],
];

function runSingleTest(caseData)
{
    var drag = caseData[0];
    var drop = caseData[1];
    var verifyFunction = caseData[2];
    var verifyParameter = caseData[3];

    expectedTextEventData = caseData[4];

    clearTargets();
    drag();
    drop();
    shouldBe("actualTextEventData", toStringLiteral(expectedTextEventData));
    verifyFunction(verifyParameter);
}

eventSender.dragMode = false;

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
