description("This tests Element.getBoundingClientRects and getClientRects positions when unscaled, scaled, and panned.");

const scale = 2;
const panX = 10;
const panY = 10;

var box = document.getElementById('box');

debug('unscaled');
setExpectedClientRectValues(100, 200, 100, 200, 100, 100);
verifyClientRect(box.getBoundingClientRect());
verifyClientRect(box.getClientRects()[0]);

debug('');
debug('scaled and panned');
setInitialScaleAndPanBy(scale, panX, panY);
setExpectedClientRectValues(100-panX, 200-panX, 100-panY, 200-panY, 100, 100);
verifyClientRect(box.getBoundingClientRect());
verifyClientRect(box.getClientRects()[0]);

var successfullyParsed = true;
endTest();
