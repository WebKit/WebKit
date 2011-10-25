description('Test that ensure textInput events should not fired when preceding key events are cancelled');

var targetRoot = document.createElement("div");
targetRoot.innerHTML += "<input type='text' id='targetInput' />";
targetRoot.innerHTML += "<textarea id='targetTextarea'></textarea>";
targetRoot.innerHTML += "<div id='targetEditable' contentEditable></div>";
document.body.appendChild(targetRoot);

var targetInput = document.getElementById("targetInput");
var targetTextarea = document.getElementById("targetTextarea");
var targetEditable = document.getElementById("targetEditable");

var receivedEventTarget = null;

function handleTextInput(evt)
{
    receivedEventTarget = evt.target;
}

var shouldCancel = false;

function mayCancel(evt)
{
    if (shouldCancel)
        evt.preventDefault();
}

targetInput.addEventListener("textInput", handleTextInput);
targetInput.addEventListener("keydown", mayCancel);
targetTextarea.addEventListener("textInput", handleTextInput);
targetTextarea.addEventListener("keydown", mayCancel);
targetEditable.addEventListener("textInput", handleTextInput);
targetEditable.addEventListener("keydown", mayCancel);

function setup(tocancel)
{
    receivedEventTarget = null;
    shouldCancel = tocancel;
}

function test(targetNode)
{
    window.targetNode = targetNode;
    setup(false);
    eventSender.keyDown("a");
    shouldBe("window.targetNode", "receivedEventTarget");
    setup(true);
    eventSender.keyDown("a");
    shouldBe("null", "receivedEventTarget");
}

targetInput.focus();
test(targetInput);

targetTextarea.focus();
test(targetTextarea);

window.getSelection().setBaseAndExtent(targetEditable, 0, targetEditable, 0);
test(targetEditable);

targetRoot.style.display = "none";

