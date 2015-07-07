description("This tests Range.getBoundingClientRects and getClientRects positions when unscaled, scaled, and panned.");

const scale = 2;
const panX = 10;
const panY = 10;

var box = document.getElementById('box');
function setDisplayOnDescriptionAndConsole(display) {
    document.getElementById('description').style.display = display;
    document.getElementById('console').style.display = display;
}

setDisplayOnDescriptionAndConsole('none');

// Selection includes the 100x100 "box" and 1500x1500 "filler" divs.
// The entire Range's bounds are the filler's, since the "box" is
// positioned absolutely inside it. The first client rect of the
// Range is the box.
document.execCommand("SelectAll");
var range = window.getSelection().getRangeAt(0);
shouldBe('range.getClientRects().length', '2');

debug('unscaled');
setExpectedClientRectValues(0, 1500, 0, 1500, 1500, 1500);
verifyClientRect(range.getBoundingClientRect());
verifyClientRect(range.getClientRects()[1]);
setExpectedClientRectValues(100, 200, 100, 200, 100, 100);
verifyClientRect(range.getClientRects()[0]);

debug('');
debug('scaled and panned');
setInitialScaleAndPanBy(scale, panX, panY);
setExpectedClientRectValues(0-panX, 1500-panX, 0-panY, 1500-panY, 1500, 1500);
verifyClientRect(range.getBoundingClientRect());
verifyClientRect(range.getClientRects()[1]);
setExpectedClientRectValues(100-panX, 200-panX, 100-panY, 200-panY, 100, 100);
verifyClientRect(range.getClientRects()[0]);

setDisplayOnDescriptionAndConsole('block');

var successfullyParsed = true;
endTest();
