// Compare sections of a <canvas> to assert they are identical.
function compareRows(ctx, y0, y1, width, height) {
    var data0 = ctx.getImageData(0, y0, width, height).data;
    var data1 = ctx.getImageData(0, y1, width, height).data;
    for (var i = 0, il = data0.length; i < il; ++i) {
        if (data0[i] != data1[i]) {
            testFailed("Pixel at " + i + " should be " + data0[i] + " but was " + data1[i]);
            break;
        }
    }
}

description("Test the default lineWidth is consistent.");

ctx = document.getElementById("canvas").getContext("2d");

ctx.strokeStyle = 'blue';

for (var j = 0; j < 3; ++j) {
    ctx.beginPath();
    for (var i = 0; i < 60; ++i) {
        var x = i * 10;
        var y = j * 100 + 30 + (i % 15);
        if (i == 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    }
    ctx.stroke();

    if (j == 0) {
        shouldBe("ctx.lineWidth", "1");
        ctx.lineWidth = ctx.lineWidth;
        shouldBe("ctx.lineWidth", "1");
    } else {
        shouldBe("ctx.lineWidth", "1");
        ctx.lineWidth = 1;
        shouldBe("ctx.lineWidth", "1");
    }
}

// Make sure that all rows are identical.
compareRows(ctx, 0, 100, 600, 100);
compareRows(ctx, 0, 200, 600, 100);

var successfullyParsed = true;
