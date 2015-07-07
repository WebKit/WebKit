description('Ensure safety on side-effect on drop-initiated TextEvent.');

function toStringLiteral(str)
{
   return "'" + str + "'";
}

var testSourceRoot = document.createElement("div");
document.body.appendChild(testSourceRoot);

var testTargetRoot = document.createElement("div");
testTargetRoot.innerHTML += "<div><span id='targetEditable' contentEditable>initialValue</span></div>";
testTargetRoot.innerHTML += "<iframe id='targetIFrame' src='data:text/html;charset=utf-8," + encodeURI("<html></html>") + "'></iframe>";
document.body.appendChild(testTargetRoot);

testTargetEditable = document.getElementById("targetEditable");
testTargetIFrame = document.getElementById("targetIFrame");
testTargetIFrameDocument = testTargetIFrame.contentDocument;
testTargetIFrameDocument.body.innerHTML = "initialBody";
testTargetIFrameDocument.designMode = "on";

function handlerRemovingTarget(event)
{
    event.target.parentNode.removeChild(event.target);
}

function handlerRemovingIFrame(event)
{
    testTargetIFrame.parentNode.removeChild(testTargetIFrame);
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
    var selection = window.getSelection();

    testSourceRoot.innerHTML = "<input type='text' value='PlainHello' id='src' />";
    var input = document.getElementById("src");
    input.focus();
    selection.modify("extend", "forward", "line");
    dragFrom(input);
}

function dropToTargetEditable()
{
    dropTo(testTargetEditable);
}

function dropToTargetIFrame()
{
    dropTo(testTargetIFrame);
}

testTargetEditable.addEventListener("textInput", handlerRemovingTarget);
dragPlainText();
dropToTargetEditable();
// detached node shouldn't get dropped value
shouldBe("testTargetEditable.innerHTML", toStringLiteral("initialValue"));

testTargetIFrameDocument.body.addEventListener("textInput", handlerRemovingIFrame);
dragPlainText();
dropToTargetIFrame();
// detached frame shouldn't get dropped value
shouldBe("testTargetIFrameDocument.body.innerHTML", toStringLiteral("initialBody"));

// Hides dataset to make dump clean.
testTargetRoot.style.display = "none";
testSourceRoot.style.display = "none";

var successfullyParsed = true;
