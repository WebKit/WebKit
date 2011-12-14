description('Test that middle click triggers the onpaste event');

var pasteCount = 0;

function handlePaste(eventObj)
{
    pasteCount++;
}

var target = document.getElementById("description");
target.onpaste = handlePaste;

if (window.layoutTestController)
{
    layoutTestController.dumpAsText();
    var x = target.offsetParent.offsetLeft + target.offsetLeft + 
        target.offsetWidth / 2;
    var y = target.offsetParent.offsetTop + target.offsetTop + 
        target.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown(1);
    eventSender.mouseUp(1);
    shouldBe("pasteCount", "1");
}

var successfullyParsed = true;
