description("Series of tests to ensure correct results of the winding rule in isPointInPath.");


var tmpimg = document.createElement('canvas');
tmpimg.width = 200;
tmpimg.height = 200;
ctx = tmpimg.getContext('2d');

// Create the image for blending test with images.
var img = document.createElement('canvas');
img.width = 100;
img.height = 100;
var imgCtx = img.getContext('2d');

// Execute test.
function prepareTestScenario() {
    debug('Testing default isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(50, 50)");             
    debug('');

    debug('Testing nonzero isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(50, 50, 'nonzero')");
    debug('');
	
    debug('Testing evenodd isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeFalse("ctx.isPointInPath(50, 50, 'evenodd')");             
    debug('');
}

// Run test and allow variation of results.
prepareTestScenario();
