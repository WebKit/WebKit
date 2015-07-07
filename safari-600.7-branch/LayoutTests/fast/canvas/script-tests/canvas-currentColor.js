description("Test that CanvasRenderingContext2D supports the 'currentColor' value.");

canvas = document.createElement('canvas');
canvas.width = 100;
canvas.height = 100;
ctx = canvas.getContext('2d');

function attachCanvasToDocument() {
    document.body.appendChild(canvas);
    return document.body.parentNode != null;
}

function tryLinearGradientColor(color) {
    var gradient = ctx.createLinearGradient(0, 0, 100, 100);
    gradient.addColorStop(0, color);
    gradient.addColorStop(1, color);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, 100, 100);
    var data = ctx.getImageData(0, 0, 1, 1).data;
    return '' + data[0] + ',' + data[1] + ',' + data[2] + ',' + data[3];
}

function tryRadialGradientColor(color) {
    var gradient = ctx.createRadialGradient(0, 0, 100, 100, 100, 100);
    gradient.addColorStop(0, color);
    gradient.addColorStop(1, color);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, 100, 100);
    var data = ctx.getImageData(0, 0, 1, 1).data;
    return '' + data[0] + ',' + data[1] + ',' + data[2] + ',' + data[3];
}

// First we test with the canvas out-of-document, 'currentColor' should mean black
shouldBe("ctx.shadowColor = '#f00'; ctx.shadowColor", "'#ff0000'");
shouldBe("ctx.shadowColor = 'currentColor'; ctx.shadowColor", "'#000000'");
shouldBe("ctx.fillStyle = '#f00'; ctx.fillStyle", "'#ff0000'");
shouldBe("ctx.fillStyle = 'currentColor'; ctx.fillStyle", "'#000000'");
shouldBe("ctx.strokeStyle = '#f00'; ctx.strokeStyle", "'#ff0000'");
shouldBe("ctx.strokeStyle = 'currentColor'; ctx.strokeStyle", "'#000000'");
shouldBe("ctx.setShadow(0, 0, 0, '#f00'); ctx.shadowColor", "'#ff0000'");
shouldBe("ctx.setShadow(0, 0, 0, 'currentColor'); ctx.shadowColor", "'#000000'");
shouldBe("ctx.setShadow(0, 0, 0, '#f00', 0.0); ctx.shadowColor", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setShadow(0, 0, 0, 'currentColor', 0.0); ctx.shadowColor", "'rgba(0, 0, 0, 0)'");
shouldBe("ctx.setStrokeColor('#f00'); ctx.strokeStyle", "'#ff0000'");
shouldBe("ctx.setStrokeColor('currentColor'); ctx.strokeStyle", "'#000000'");
shouldBe("ctx.setStrokeColor('#f00', 0.0); ctx.strokeStyle", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setStrokeColor('currentColor', 0.0); ctx.strokeStyle", "'rgba(0, 0, 0, 0)'");
shouldBe("ctx.setFillColor('#f00'); ctx.fillStyle", "'#ff0000'");
shouldBe("ctx.setFillColor('currentColor'); ctx.fillStyle", "'#000000'");
shouldBe("ctx.setFillColor('#f00', 0.0); ctx.fillStyle", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setFillColor('currentColor', 0.0); ctx.fillStyle", "'rgba(0, 0, 0, 0)'");
shouldBe("tryLinearGradientColor('#f00')", "'255,0,0,255'");
shouldBe("tryLinearGradientColor('currentColor')", "'0,0,0,255'");
shouldBe("tryRadialGradientColor('#f00')", "'255,0,0,255'");
shouldBe("tryRadialGradientColor('currentColor')", "'0,0,0,255'");

// Attach to the document and set the canvas's color to #123456
shouldBe("attachCanvasToDocument()", "true");
shouldBe("canvas.style.color = '#123456'; canvas.style.color", "'rgb(18, 52, 86)'");

// 'currentColor' should now mean #123456
shouldBe("ctx.shadowColor = '#f00'; ctx.shadowColor", "'#ff0000'");
shouldBe("ctx.shadowColor = 'currentColor'; ctx.shadowColor", "'#123456'");
shouldBe("ctx.fillStyle = '#f00'; ctx.fillStyle", "'#ff0000'");
shouldBe("ctx.fillStyle = 'currentColor'; ctx.fillStyle", "'#123456'");
shouldBe("ctx.strokeStyle = '#f00'; ctx.strokeStyle", "'#ff0000'");
shouldBe("ctx.strokeStyle = 'currentColor'; ctx.strokeStyle", "'#123456'");
shouldBe("ctx.setShadow(0, 0, 0, '#f00'); ctx.shadowColor", "'#ff0000'");
shouldBe("ctx.setShadow(0, 0, 0, 'currentColor'); ctx.shadowColor", "'#123456'");
shouldBe("ctx.setShadow(0, 0, 0, '#f00', 0.0); ctx.shadowColor", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setShadow(0, 0, 0, 'currentColor', 0.0); ctx.shadowColor", "'rgba(18, 52, 86, 0)'");
shouldBe("ctx.setStrokeColor('#f00'); ctx.strokeStyle", "'#ff0000'");
shouldBe("ctx.setStrokeColor('currentColor'); ctx.strokeStyle", "'#123456'");
shouldBe("ctx.setStrokeColor('#f00', 0.0); ctx.strokeStyle", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setStrokeColor('currentColor', 0.0); ctx.strokeStyle", "'rgba(18, 52, 86, 0)'");
shouldBe("ctx.setFillColor('#f00'); ctx.fillStyle", "'#ff0000'");
shouldBe("ctx.setFillColor('currentColor'); ctx.fillStyle", "'#123456'");
shouldBe("ctx.setFillColor('#f00', 0.0); ctx.fillStyle", "'rgba(255, 0, 0, 0)'");
shouldBe("ctx.setFillColor('currentColor', 0.0); ctx.fillStyle", "'rgba(18, 52, 86, 0)'");
shouldBe("tryLinearGradientColor('#f00')", "'255,0,0,255'");
shouldBe("tryLinearGradientColor('currentColor')", "'0,0,0,255'");
shouldBe("tryRadialGradientColor('#f00')", "'255,0,0,255'");
shouldBe("tryRadialGradientColor('currentColor')", "'0,0,0,255'");

// Last but not least, verify that we're case insensitive
shouldBe("ctx.shadowColor = '#f00'; ctx.shadowColor", "'#ff0000'");
shouldBe("ctx.shadowColor = 'CURRENTCOLOR'; ctx.shadowColor", "'#123456'");
