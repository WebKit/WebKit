description('This tests "scroll" offsets when unscaled, scaled, and panned.');

const scale = 2;
const panX = 10;
const panY = 10;

debug('unscaled');
setExpectedWindowSize(800, 600);
verifyWindowSize();

debug('');
debug('scaled and panned');
setInitialScaleAndPanBy(scale, panX, panY);
setExpectedWindowSize(800/scale, 600/scale);
verifyWindowSize();

var successfullyParsed = true;
endTest();
