// Parsing JSON payloads dominated by int32 numbers, e.g. time series metrics or tilemap data.

var payloads = [];
var arrays = [];
for (var k = 0; k < 16; k++) {
    var pts = [];
    for (var j = 0; j < 1024; j++)
        pts.push(((j * 2654435761 + k * 97) >>> 0) % 100000);
    arrays.push(pts);
    payloads.push(JSON.stringify({ series: "cpu", points: pts }));
}

var iterations = 30000;

var expected = 0;
for (var i = 0; i < iterations; i++)
    expected += 1024 + arrays[i & 15][i & 1023];

var sum = 0;
for (var i = 0; i < iterations; i++) {
    var o = JSON.parse(payloads[i & 15]);
    sum += o.points.length + o.points[i & 1023];
}

if (sum !== expected)
    throw new Error("bad result: " + sum + " vs " + expected);
