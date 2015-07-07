description('Test addPath of Path2D.');

function refTest(result, expected) {
    if (!result.length)
        return true;
    if (result.length != expected.length)
        return false;
    for (var i = 0; i < result.length; ++i) {
        if (result[i] != expected[i])
            return false;
    }
    return true;
}

var ctx = document.createElement('canvas').getContext('2d');
document.body.appendChild(ctx.canvas);
ctx.fillStyle = 'green';
var matrix = document.createElementNS('http://www.w3.org/2000/svg','svg').createSVGMatrix();


debug('Add path B with path data to path A without path data.');
var pathA = new Path2D();
var pathB = new Path2D();
pathB.rect(0,0,100,100);
pathA.addPath(pathB);
ctx.fill(pathA);

// Compare results with reference drawing.
var result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.beginPath();
ctx.fillRect(0,0,100,100);
var expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add empty path B to path A with path data.');
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathA.addPath(pathB);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.beginPath();
ctx.fillRect(0,0,100,100);
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path with path data B to path A with path data.');
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
pathA.addPath(pathB);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.rect(20,20,100,100);
ctx.fill();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path with path data B to path A with path data. Fill with winding rule evenodd.');
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
pathA.addPath(pathB);
ctx.fill(pathA, 'evenodd');

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.rect(20,20,100,100);
ctx.fill('evenodd');
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path B to path A with transformation matrix.');
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
matrix = matrix.translate(30,-20);
matrix = matrix.scale(1.5,1.5);
pathA.addPath(pathB, matrix);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.translate(30,-20);
ctx.scale(1.5,1.5);
ctx.rect(20,20,100,100);
ctx.fill();
ctx.restore();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path A to itself with transformation matrix.');
pathA = new Path2D();
pathA.rect(0,0,100,100);
matrix.a = 1;
matrix.b = 0;
matrix.c = 0;
matrix.d = 1;
matrix.e = 20;
matrix.f = 20;
pathA.addPath(pathA, matrix);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.translate(20,20);
ctx.rect(0,0,100,100);
ctx.fill();
ctx.restore();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path B to path A with singular transformation matrix (1).');
matrix.a = 0;
matrix.b = 0;
matrix.c = 0;
matrix.d = 0;
matrix.e = 0;
matrix.f = 0;
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
pathA.addPath(pathB, matrix);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.fill();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path B to path A with singular transformation matrix (2).');
matrix.a = Math.NaN;
matrix.b = Math.NaN;
matrix.c = Math.NaN;
matrix.d = Math.NaN;
matrix.e = Math.NaN;
matrix.f = Math.NaN;
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
pathA.addPath(pathB, matrix);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.fill();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Add path B to path A with singular transformation matrix (3).');
matrix.a = Math.Infinity;
matrix.b = Math.Infinity;
matrix.c = Math.Infinity;
matrix.d = Math.Infinity;
matrix.e = Math.Infinity;
matrix.f = Math.Infinity;
pathA = new Path2D();
pathA.rect(0,0,100,100);
pathB = new Path2D();
pathB.rect(20,20,100,100);
pathA.addPath(pathB, matrix);
ctx.fill(pathA);

// Compare results with reference drawing.
result = ctx.getImageData(0, 0, 150, 150);
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.fill();
expected = ctx.getImageData(0, 0, 150, 150);
shouldBe('refTest(result.data, expected.data)', 'true');
ctx.clearRect(0,0,300,150);
debug('');


debug('Various tests of invalid values.');
matrix.a = 1;
matrix.b = 0;
matrix.c = 0;
matrix.d = 1;
matrix.e = 0;
matrix.f = 0;
pathA = new Path2D();
pathB = new Path2D();
shouldThrow('pathA.addPath(matrix, pathB)');
shouldThrow('pathA.addPath(pathB, ctx.canvas)');
shouldThrow('pathA.addPath(pathB, null)');
shouldThrow('pathA.addPath(pathB, undefined)');
shouldThrow('pathA.addPath(pathB, 0)');
shouldThrow('pathA.addPath(pathB, "0")');

