description("Series of tests to ensure correct results of the winding rule.");


var tmpimg = document.createElement('canvas');
tmpimg.width = 200;
tmpimg.height = 200;
ctx = tmpimg.getContext('2d');

// Create the image for blending test with images.
var img = document.createElement('canvas');
img.width = 100;
img.height = 100;
var imgCtx = img.getContext('2d');

function pixelDataAtPoint()
{
  return ctx.getImageData(50, 50, 1, 1).data;
}

function checkResult(expectedColors, sigma) {
    for (var i = 0; i < 4; i++)
	    shouldBeCloseTo("pixelDataAtPoint()[" + i + "]", expectedColors[i], sigma);
}

// Execute test.
function prepareTestScenario() {
    debug('Testing default fill');
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.fillStyle = 'rgb(0,255,0)';
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    ctx.fill();
    checkResult([0, 255, 0, 255], 5);                        
    debug('');

    debug('Testing nonzero fill');
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.fillStyle = 'rgb(0,255,0)';
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    ctx.fill('nonzero');
    checkResult([0, 255, 0, 255], 5);
    debug('');
	
    debug('Testing evenodd fill');
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.fillStyle = 'rgb(0,255,0)';
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    ctx.fill('evenodd');
    checkResult([255, 0, 0, 255], 5);                        
    debug('');

}

// Run test and allow variation of results.
prepareTestScenario();
