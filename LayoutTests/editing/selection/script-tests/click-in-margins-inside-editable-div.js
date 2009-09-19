description("Ensure cursor placement matches IE6/IE7/FF3 when clicking above/below lines in padding.")

document.body.style.margin = 0;

var div = document.createElement("div");
div.style.cssText = "font-family: ahem; font-size: 20px; -webkit-text-fill-color: yellow; width: 40px; height: 80px; padding: 20px; background-color: green;";
div.contentEditable = true;

var firstText = document.createTextNode("XX");
var firstDiv = document.createElement("div");
firstDiv.appendChild(firstText);
firstDiv.style.cssText = "padding-bottom: 19px; border-bottom: 1px solid pink; margin-bottom: 20px";
div.appendChild(firstDiv);

var secondText = document.createTextNode("YY");
var secondDiv = document.createElement("div");
secondDiv.appendChild(secondText);
div.appendChild(secondDiv);

document.body.insertBefore(div, document.body.firstChild);

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

// When clicking between divs separated by margins in an editable
// text region, the browsers differ in behavior:
// FF3: vertical padding box + some sort of overhang decent?
// IE6/IE7: vertical padding box
// IE8 b2: margin box
// Safari 3: margin box (but cursor goes at end of line)

// We're going to follow IE 7's behavior and use the padding box height
// for determining if the click below a div (in the padding/margin region)

// The rules for clicking above or below the text are different on Windows and Mac.
// Later we could break this into two tests, one that tests each platform's rules,
// since this is supported as a setting in the Settings object, but for now we'll
// just use separate expected results for Mac and other platforms.
var expectMacStyleSelection = navigator.userAgent.search(/\bMac OS X\b/) != -1;

clickShouldResultInRange(10, 10, firstText, 0);
clickShouldResultInRange(40, 10, firstText, expectMacStyleSelection ? 0 : 1);
clickShouldResultInRange(70, 10, firstText, expectMacStyleSelection ? 0 : 2);

clickShouldResultInRange(10, 30, firstText, 0);
clickShouldResultInRange(70, 30, firstText, 2);

clickShouldResultInRange(10, 50, firstText, expectMacStyleSelection ? 2 : 0);
clickShouldResultInRange(40, 50, firstText, expectMacStyleSelection ? 2 : 1);
clickShouldResultInRange(70, 50, firstText, 2);

clickShouldResultInRange(10, 70, secondText, 0);
clickShouldResultInRange(40, 70, secondText, expectMacStyleSelection ? 0 : 1);
clickShouldResultInRange(70, 70, secondText, expectMacStyleSelection ? 0 : 2);

clickShouldResultInRange(10, 110, secondText, expectMacStyleSelection ? 2 : 0);
clickShouldResultInRange(40, 110, secondText, expectMacStyleSelection ? 2 : 1);
clickShouldResultInRange(70, 110, secondText, 2);

// Clean up after ourselves if we're not being run in the browser
if (window.eventSender) {
    document.body.removeChild(div);
} else {
    runInteractiveTests();
}

var successfullyParsed = true;
