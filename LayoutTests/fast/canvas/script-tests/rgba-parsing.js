description("Test rgba() color parsing results");

var ctx = document.createElement('canvas').getContext('2d');

function parse(rgba) {
    ctx.globalCompositeOperation = 'copy';
    ctx.fillStyle = '#666';
    ctx.fillStyle = rgba;
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    var idata = ctx.getImageData(0, 0, 1, 1);
    var data = idata.data;
    return "RGBA[" + data[0] + ", " + data[1] + ", " + data[2] + ", " + data[3] + "]";
}

shouldBe("parse('rgba(0, 0, 0, -0.10)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, -5.0)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, 5.0)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, -1)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, 0)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, 2)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 2.0)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 0.0)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, 00.0)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, 0.00)')", "'RGBA[0, 0, 0, 0]'");
shouldBe("parse('rgba(0, 0, 0, .1)')", "'RGBA[0, 0, 0, 25]'");
shouldBe("parse('rgba(0, 0, 0, .2)')", "'RGBA[0, 0, 0, 51]'");
shouldBe("parse('rgba(0, 0, 0, .3)')", "'RGBA[0, 0, 0, 76]'");
shouldBe("parse('rgba(0, 0, 0, .4)')", "'RGBA[0, 0, 0, 102]'");
shouldBe("parse('rgba(0, 0, 0, .5)')", "'RGBA[0, 0, 0, 127]'");
shouldBe("parse('rgba(0, 0, 0, .6)')", "'RGBA[0, 0, 0, 153]'");
shouldBe("parse('rgba(0, 0, 0, .7)')", "'RGBA[0, 0, 0, 179]'");
shouldBe("parse('rgba(0, 0, 0, .8)')", "'RGBA[0, 0, 0, 204]'");
shouldBe("parse('rgba(0, 0, 0, .9)')", "'RGBA[0, 0, 0, 230]'");
shouldBe("parse('rgba(0, 0, 0, 0.1)')", "'RGBA[0, 0, 0, 25]'");
shouldBe("parse('rgba(0, 0, 0, 0.2)')", "'RGBA[0, 0, 0, 51]'");
shouldBe("parse('rgba(0, 0, 0, 0.3)')", "'RGBA[0, 0, 0, 76]'");
shouldBe("parse('rgba(0, 0, 0, 0.4)')", "'RGBA[0, 0, 0, 102]'");
shouldBe("parse('rgba(0, 0, 0, 0.5)')", "'RGBA[0, 0, 0, 127]'");
shouldBe("parse('rgba(0, 0, 0, 0.6)')", "'RGBA[0, 0, 0, 153]'");
shouldBe("parse('rgba(0, 0, 0, 0.7)')", "'RGBA[0, 0, 0, 179]'");
shouldBe("parse('rgba(0, 0, 0, 0.8)')", "'RGBA[0, 0, 0, 204]'");
shouldBe("parse('rgba(0, 0, 0, 0.9)')", "'RGBA[0, 0, 0, 230]'");
shouldBe("parse('rgba(0, 0, 0, 1.0)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 0.10)')", "'RGBA[0, 0, 0, 25]'");
shouldBe("parse('rgba(0, 0, 0, 0.20)')", "'RGBA[0, 0, 0, 51]'");
shouldBe("parse('rgba(0, 0, 0, 0.30)')", "'RGBA[0, 0, 0, 76]'");
shouldBe("parse('rgba(0, 0, 0, 0.40)')", "'RGBA[0, 0, 0, 102]'");
shouldBe("parse('rgba(0, 0, 0, 0.50)')", "'RGBA[0, 0, 0, 127]'");
shouldBe("parse('rgba(0, 0, 0, 0.60)')", "'RGBA[0, 0, 0, 153]'");
shouldBe("parse('rgba(0, 0, 0, 0.70)')", "'RGBA[0, 0, 0, 179]'");
shouldBe("parse('rgba(0, 0, 0, 0.80)')", "'RGBA[0, 0, 0, 204]'");
shouldBe("parse('rgba(0, 0, 0, 0.90)')", "'RGBA[0, 0, 0, 230]'");
shouldBe("parse('rgba(0, 0, 0, 1.00)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, .10)')", "'RGBA[0, 0, 0, 25]'");
shouldBe("parse('rgba(0, 0, 0, .20)')", "'RGBA[0, 0, 0, 51]'");
shouldBe("parse('rgba(0, 0, 0, .30)')", "'RGBA[0, 0, 0, 76]'");
shouldBe("parse('rgba(0, 0, 0, .40)')", "'RGBA[0, 0, 0, 102]'");
shouldBe("parse('rgba(0, 0, 0, .50)')", "'RGBA[0, 0, 0, 127]'");
shouldBe("parse('rgba(0, 0, 0, .60)')", "'RGBA[0, 0, 0, 153]'");
shouldBe("parse('rgba(0, 0, 0, .70)')", "'RGBA[0, 0, 0, 179]'");
shouldBe("parse('rgba(0, 0, 0, .80)')", "'RGBA[0, 0, 0, 204]'");
shouldBe("parse('rgba(0, 0, 0, .90)')", "'RGBA[0, 0, 0, 230]'");
shouldBe("parse('rgba(0, 0, 0, 0.10000000000000000000000)')", "'RGBA[0, 0, 0, 25]'");
shouldBe("parse('rgba(0, 0, 0, 0.20000000000000000000000)')", "'RGBA[0, 0, 0, 51]'");
shouldBe("parse('rgba(0, 0, 0, 0.30000000000000000000000)')", "'RGBA[0, 0, 0, 76]'");
shouldBe("parse('rgba(0, 0, 0, 0.40000000000000000000000)')", "'RGBA[0, 0, 0, 102]'");
shouldBe("parse('rgba(0, 0, 0, 0.50000000000000000000000)')", "'RGBA[0, 0, 0, 127]'");
shouldBe("parse('rgba(0, 0, 0, 0.60000000000000000000000)')", "'RGBA[0, 0, 0, 153]'");
shouldBe("parse('rgba(0, 0, 0, 0.70000000000000000000000)')", "'RGBA[0, 0, 0, 179]'");
shouldBe("parse('rgba(0, 0, 0, 0.80000000000000000000000)')", "'RGBA[0, 0, 0, 204]'");
shouldBe("parse('rgba(0, 0, 0, 0.90000000000000000000000)')", "'RGBA[0, 0, 0, 230]'");
shouldBe("parse('rgba(0, 0, 0, 1.00000000000000000000000)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 0.990)')", "'RGBA[0, 0, 0, 253]'");
shouldBe("parse('rgba(0, 0, 0, 0.991)')", "'RGBA[0, 0, 0, 253]'");
shouldBe("parse('rgba(0, 0, 0, 0.992)')", "'RGBA[0, 0, 0, 253]'");
shouldBe("parse('rgba(0, 0, 0, 0.993)')", "'RGBA[0, 0, 0, 254]'");
shouldBe("parse('rgba(0, 0, 0, 0.994)')", "'RGBA[0, 0, 0, 254]'");
shouldBe("parse('rgba(0, 0, 0, 0.995)')", "'RGBA[0, 0, 0, 254]'");
shouldBe("parse('rgba(0, 0, 0, 0.996)')", "'RGBA[0, 0, 0, 254]'");
shouldBe("parse('rgba(0, 0, 0, 0.997)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 0.998)')", "'RGBA[0, 0, 0, 255]'");
shouldBe("parse('rgba(0, 0, 0, 0.999)')", "'RGBA[0, 0, 0, 255]'");

// Dump a bunch of colors
for (g = 0; g < 256; g += 63) {
    for (a = 0.0; a < 1.0; a += 0.01333) {
        rgba = 'rgba(' + g + ', ' + g + ', ' + g + ', ' + a + ')';
        result = parse(rgba);
        debug(rgba + ' => ' + result);
    }
}

var successfullyParsed = true;
