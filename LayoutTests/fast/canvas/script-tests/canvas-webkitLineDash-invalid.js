description("Test for invalid input of webkitLineDash and webkitLineDashOffset");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '700');
var ctx = canvas.getContext('2d');

function trySettingLineDash(value) {
    ctx.webkitLineDash = [1.5, 2.5];
    ctx.webkitLineDash = value;
    return ctx.webkitLineDash;
}

function trySettingLineDashOffset(value) {
    ctx.webkitLineDashOffset = 1.5;
    ctx.webkitLineDashOffset = value;
    return ctx.webkitLineDashOffset;
}

shouldBe("trySettingLineDash([1, 2, 3])", "[1, 2, 3]");
shouldBe("trySettingLineDash([1, -1])", "[1.5, 2.5]");
shouldBe("trySettingLineDash([1, Infinity])", "[1.5, 2.5]");
shouldBe("trySettingLineDash([1, -Infinity])", "[1.5, 2.5]");
shouldBe("trySettingLineDash([1, NaN])", "[1.5, 2.5]");
shouldBe("trySettingLineDash([1, 'string'])", "[1.5, 2.5]");

shouldBe("trySettingLineDashOffset(0.5)", "0.5");
shouldBe("trySettingLineDashOffset(-0.5)", "-0.5");
shouldBe("trySettingLineDashOffset(Infinity)", "1.5");
shouldBe("trySettingLineDashOffset(-Infinity)", "1.5");
shouldBe("trySettingLineDashOffset(NaN)", "1.5");
shouldBe("trySettingLineDashOffset('string')", "1.5");
