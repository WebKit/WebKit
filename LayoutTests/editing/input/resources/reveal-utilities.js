function offsetFromViewportTop(element)
{
    return element.getClientRects()[0].top;
}

function offsetOfMiddleFromViewportTop(element)
{
    return element.getClientRects()[0].top + Math.round(element.getClientRects()[0].height / 2);
}

function copyText()
{
    var copy = document.getElementById("copy");
    copy.focus();
    document.execCommand("selectall");
    document.execCommand("copy");
}

function generateNumbers(from, to, zeroPaddedWidth, delimiter)
{
    var result = "";
    for (var i = from; i <= to; ++i) {
        var number = i.toString();
        while (number.length < zeroPaddedWidth)
            number = "0" + number;
        result += number + delimiter;
    }
    return result;
}

function assertInputIsInTheMiddleOfViewport()
{
    var viewportMiddle = Math.round(window.innerHeight / 2);
    var offsetOfInput = offsetOfMiddleFromViewportTop(document.getElementById("input"));
    document.getElementById("results").innerHTML += "ScrollVertically: " +
        (Math.abs(offsetOfInput - viewportMiddle) <= 3 ?
         "PASS" :
         "FAIL<br>viewportMiddle: " + viewportMiddle + ", offsetOfInput: " + offsetOfInput);
}

function performVerticalScrollingTest()
{
    var initialOffset = offsetFromViewportTop(document.body.children[0]);
    document.getElementById("input").focus();
    if (window.eventSender) {
        while (offsetFromViewportTop(document.body.children[0]) < initialOffset)
            eventSender.keyDown("pageUp");
        return true;
    }
    return false;
}

function performVerticalScrollingInputTest()
{
    if (performVerticalScrollingTest())
        eventSender.keyDown("a");
}

function performVerticalScrollingPasteTest()
{
    copyText();
    if (performVerticalScrollingTest()) {
        document.execCommand("selectall");
        document.execCommand("paste");
    }
}
