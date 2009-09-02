description("This tests CompositeEditCommand::breakOutOfEmptyListItem by inserting new paragraph")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function pressKey(key)
{
    if (window.KeyEvent) {
        var ev = document.createEvent("KeyboardEvent");
        ev.initKeyEvent("keypress", true, true, window,  0,0,0,0, 0, key.charCodeAt(0));
        document.body.dispatchEvent(ev);
    }
    else {
        var ev = document.createEvent("TextEvent");
        ev.initTextEvent('textInput', true, true, null, key.charAt(0));
        document.body.dispatchEvent(ev);
    }
}

function enterAtTarget(initialContent)
{
    testContainer.innerHTML = initialContent;
    var r = document.createRange();
    var s = window.getSelection();

    var t = document.getElementById('target');
    if (!t)
        return 'target element not found';
    r.setStart(t, 0);
    r.setEnd(t, 0);
    s.removeAllRanges();
    s.addRange(r);

    pressKey('\n');
    
    return testContainer.innerHTML;
}

function testBreakOutOfEmptyListItem(initialContents, expectedContents)
{
    shouldBe("enterAtTarget('"+initialContents+"')", "'"+expectedContents+"'");
}

testBreakOutOfEmptyListItem('<ul><li>a <ul><li>b</li><li id="target"></li></ul> </li></ul>', '<ul><li>a </li><ul><li>b</li></ul><li><br></li> </ul>');
testBreakOutOfEmptyListItem('<ul><li>a <ul><li id="target"></li><li>b</li></ul> </li></ul>', '<ul><li>a </li><li><br></li><ul><li>b</li></ul> </ul>');
testBreakOutOfEmptyListItem('<ul><li>a <ul><li>b</li><li id="target"></li><li>c</li></ul> </li></ul>', '<ul><li>a </li><ul><li>b</li></ul><li><br></li><ul><li>c</li></ul> </ul>');
testBreakOutOfEmptyListItem('<ul><li>hello<ul><li id="target"><br></li></ul>world</li></ul>', '<ul><li>hello<div><br></div>world</li></ul>');
testBreakOutOfEmptyListItem('<ul><li>hello<ul><li id="target"><br></li></ul></li></ul>', '<ul><li>hello</li><li><br></li></ul>');
testBreakOutOfEmptyListItem('<ul><li><ul><li id="target"><br></li></ul>world</li></ul>', '<ul><li><div><br></div>world</li></ul>');
testBreakOutOfEmptyListItem('<ul><li><ul><li id="target"><br></li></ul></li></ul>', '<ul><li></li><li><br></li></ul>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
