description('Test constructors of ImageData.');

ctx = document.createElement('canvas').getContext('2d');
ctx.fillStyle = 'green';
ctx.fillRect(0,0,10,10);
var imageData = ctx.getImageData(0,0,10,10);

debug('Test invalid ImageData constructor arguments.')
shouldThrowErrorName('new ImageData()', 'TypeError');
shouldThrowErrorName('new ImageData(1)', 'TypeError');
shouldThrowErrorName('new ImageData(new Uint8ClampedArray([1,2,3,4]));', 'TypeError');
shouldThrowErrorName('new ImageData(0, 0)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(20, 0)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(0, 20)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(-20, 20)', 'TypeError');
shouldThrowErrorName('new ImageData(20, -20)', 'TypeError');
shouldThrowErrorName('new ImageData(null, 20)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(32768, 32768)', 'TypeError');
shouldThrowErrorName('new ImageData(null, 20, 20)', 'TypeError');
shouldThrowErrorName('new ImageData(imageData, 20, 20)', 'TypeError');
shouldThrowErrorName('new ImageData(imageData, 0)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(imageData, 20, 0)', 'TypeError');
shouldThrowErrorName('new ImageData(imageData, 0, 20)', 'TypeError');
shouldThrowErrorName('new ImageData(imageData, 10, 5)', 'TypeError');
shouldThrowErrorName('new ImageData(imageData.data, 10, 5)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(imageData.data, -10, 5)', 'InvalidStateError');
shouldThrowErrorName('new ImageData(imageData.data, 10, -10)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(new Uint8ClampedArray([1,2,3,4,5,6,7,8]),536870913,2);', 'InvalidStateError');
shouldThrowErrorName('new ImageData({},2,2);', 'TypeError');
shouldThrowErrorName('new ImageData(undefined,2,2);', 'TypeError');
shouldThrowErrorName('new ImageData("none",2,2);', 'TypeError');
shouldThrowErrorName('new ImageData(0,2,2);', 'TypeError');
shouldThrowErrorName('new ImageData(imageData.data, 32768, 32768)', 'InvalidStateError');
shouldThrowErrorName('new ImageData(imageData.data, Infinity, Infinity)', 'IndexSizeError');
shouldThrowErrorName('new ImageData(imageData.data, NaN, NaN)', 'IndexSizeError');
debug('');

debug('Test valid ImageData constructors.')
new ImageData(10, 10);
new ImageData(imageData.data, 10, 10);
new ImageData(imageData.data, 10);
// This should throw but doesn't because of a CodeGeneratorJS bug.
new ImageData(imageData.data, 10, 0);

debug('Test that we got the pixel array from imageData.');
var newImageData = new ImageData(imageData.data, 10, 10);
shouldBe('imageData.data[1]', '128');
debug('');

debug('Test that we got a reference rather than a copy.')
newImageData.data[1] = 100;
shouldBe('imageData.data[1]','100');
debug('');

function testTransparentBlack(data) {
    for (var i = 0; i < data.length; ++i) {
        if (data[i] != 0)
            return false;
    }   
    return true;
}

var imageData2 = new ImageData(100,100);
shouldBeTrue('testTransparentBlack(imageData2.data)');

var data = new Uint8ClampedArray(400);
var imageData2 = new ImageData(data, 20, 5);
shouldBe("imageData2.data", "data");

