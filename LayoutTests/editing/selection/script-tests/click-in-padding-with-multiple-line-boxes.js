description("Click above/beneath the word FOO. Notice how the cursor goes in the right place (after the letter F).")

document.body.style.margin = 0;

var div = document.createElement('div');
div.style.cssText = "font-family:ahem; width:50px; height:60px; border:1px solid blue; white-space:nowrap; overflow:scroll; padding:50px;";
div.contentEditable = true;
div.innerHTML = "12345<span id='foo'>FOO</span>6789<span>BAR</span>";
document.body.insertBefore(div, document.body.firstChild);

div.scrollLeft = 50;

function clickShouldResultInRange(x, y, node, offset) {
    if (window.eventSender) {
        clickAt(x, y);
        assertSelectionAt(node, offset);
    } else {
        tests.push({
            testFunction: function() { assertRange(node, offset); },
            clickString: " " + x + ", " + y }
        )
    }
}

function printClickStringForWaitingTest()
{
    if (!tests[testIndex])
        return;
    debug("Waiting for click @ " + tests[testIndex].clickString);
}

var testIndex = 0;
var tests = [];
function runInteractiveTests()
{
    testIndex = 0;
    printClickStringForWaitingTest();
    document.body.addEventListener("mouseup", function() {
        debug("Got click @ " + event.clientX + ", " + event.clientY);
        var test = tests[testIndex];
        if (test) {
            test.testFunction();
            testIndex++;
            printClickStringForWaitingTest();
        }
    }, false);
}

// The rules for clicking below the text are different on Windows and Mac.
function editingTest(editingBehavior)
{
    if (window.internals)
        internals.settings.setEditingBehavior(editingBehavior);
    
    var expectMacStyleSelection = editingBehavior == "mac";

    var foo = document.getElementById('foo');
    var x = foo.offsetLeft - div.scrollLeft + 10;

    // Click 10px after the start of the span should put the cursor right after the letter F.
    if (expectMacStyleSelection) {
        clickShouldResultInRange(x, foo.offsetTop - 20, div.firstChild, 0);
        clickShouldResultInRange(x, foo.offsetTop + 20, div.lastChild.firstChild, 3);
    } else {
        clickShouldResultInRange(x, foo.offsetTop - 20, foo.firstChild, 1);
        clickShouldResultInRange(x, foo.offsetTop + 20, foo.firstChild, 1);
    }
}

editingTest("mac");
editingTest("win");
editingTest("android");

// Clean up after ourselves if we're not being run in the browser
if (window.eventSender) {
    document.body.removeChild(div);
} else {
    runInteractiveTests();
}

var successfullyParsed = true;
