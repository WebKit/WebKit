description('This tests "scroll" offsets when unscaled, scaled, and panned.');

const scale = 2;
const panX = 10;
const panY = 10;

debug('unscaled');
setExpectedScrollOffsets(0, 0);
verifyScrollOffsets();

debug('');
debug('scaled and panned');
setInitialScaleAndPanBy(scale, panX, panY);
setExpectedScrollOffsets(panX, panY);
verifyScrollOffsets();

var successfullyParsed = true;
endTest();
