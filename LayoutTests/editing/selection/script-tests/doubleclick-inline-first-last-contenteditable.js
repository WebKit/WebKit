description("Test to check if last and first word can be selected when they double-clicked (Bug 36359)");

function toLiteral(textValue)
{
    return "'" + textValue + "'";
}

function getPositionOfNode(node)
{
    var n = node;
    var offsetX = 0, offsetY = 0;

    while (n) {
        offsetX += n.offsetLeft;
        offsetY += n.offsetTop;
        n = n.offsetParnet;
    }

    return {
      "x":  offsetX,
      "y":  offsetY
    };
}

function doubleClickPosition(pos)
{
    eventSender.mouseMoveTo(pos.x, pos.y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function singleClickPosition(pos)
{
    eventSender.mouseMoveTo(pos.x, pos.y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

var targetRoot = document.createElement("div");
document.body.appendChild(targetRoot);

var scratchRoot = document.createElement("div");
document.body.appendChild(scratchRoot);

function computeTextSize(text)
{
    scratchRoot.innerHTML = ("<span id='scratch'>" + text + "</span>");
    var scratch = document.getElementById("scratch");
    return { width: scratch.offsetWidth, height: scratch.offsetHeight };
}

function testWithDoublleClick(targetInnerHTML, expectedText)
{
    var textSize = computeTextSize(expectedText);

    targetRoot.innerHTML = targetInnerHTML;
    var target = document.getElementById("target");
    var nodePosition = getPositionOfNode(target);
    doubleClickPosition({ x: nodePosition.x + textSize.width / 2, y: nodePosition.y + textSize.height / 2 });
    shouldBe("window.getSelection().toString()", toLiteral(expectedText));
}

function testWithClickAndModify(targetInnerHTML, expectedText)
{
    var selection = window.getSelection();
    targetRoot.innerHTML = targetInnerHTML;
    var target = document.getElementById("target");
    singleClickPosition(getPositionOfNode(target));

    selection.modify("extend", "forward", "word");
    var lastHalf = window.getSelection().toString();
    selection.modify("extend", "backward", "word");
    var firstHalf = window.getSelection().toString();

    window.selectedByModify = firstHalf + lastHalf;
    shouldBe("window.selectedByModify", toLiteral(expectedText));
}

testRunner.setSelectTrailingWhitespaceEnabled(false);

var shouldSelecteFirstWordInline = "<span id='target' contentEditable>selectme1</span> and not select us";
testWithDoublleClick(shouldSelecteFirstWordInline, "selectme1");
testWithClickAndModify(shouldSelecteFirstWordInline, "selectme1");

var shouldSelectLastWordInline = "you should ignore us but <span id='target' contentEditable>selectme2</span>";
testWithDoublleClick(shouldSelectLastWordInline, "selectme2");
testWithClickAndModify(shouldSelectLastWordInline, "selectme2");

var shouldSelectMiddleWordInline = "you should get <span id='target' contentEditable>selectme3</span> selected";
testWithDoublleClick(shouldSelectMiddleWordInline, "selectme3");
testWithClickAndModify(shouldSelectMiddleWordInline, "selectme3");

var shouldSelecteFirstWordBlock = "<div id='target' contentEditable>selectme4</div> and not select us";
testWithDoublleClick(shouldSelecteFirstWordBlock, "selectme4");
testWithClickAndModify(shouldSelecteFirstWordBlock, "selectme4");

var shouldSelectLastWordBlock = "you should ignore us but <div id='target' contentEditable>selectme5</div>";
testWithDoublleClick(shouldSelectLastWordBlock, "selectme5");
testWithClickAndModify(shouldSelectLastWordBlock, "selectme5");

var shouldSelectMiddleWordBlock = "you should get <div id='target' contentEditable>selectme6</div> selected";
testWithDoublleClick(shouldSelectMiddleWordBlock, "selectme6");
testWithClickAndModify(shouldSelectMiddleWordBlock, "selectme6");

targetRoot.style.display = "none";
scratchRoot.style.display = "none";

var successfullyParsed = true;
