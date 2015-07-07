description("Test for invalid input of setLineDash, getLineDash and lineDashOffset");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '700');
var ctx = canvas.getContext('2d');
var initialLineDash = [1.5, 2.5];
var initialLineDashOffset = 1.5;

function resetLineDash() {
    ctx.setLineDash(initialLineDash);
    ctx.lineDashOffset = initialLineDashOffset;
}

function trySettingLineDash(value) {
    resetLineDash();
    ctx.setLineDash(value);
    return ctx.getLineDash();
}

function trySettingLineDashWithNoArgs() {
    resetLineDash();
    ctx.setLineDash();
    return ctx.getLineDash();
}

function trySettingLineDashOffset(value) {
    resetLineDash();
    ctx.lineDashOffset = value;
    return ctx.lineDashOffset;
}

shouldBe("trySettingLineDash([1, -1])", "initialLineDash");
shouldBe("trySettingLineDash([1, Infinity])", "initialLineDash");
shouldBe("trySettingLineDash([1, -Infinity])", "initialLineDash");
shouldBe("trySettingLineDash([1, NaN])", "initialLineDash");
shouldBe("trySettingLineDash([1, 'string'])", "initialLineDash");
shouldThrow("trySettingLineDashWithNoArgs()", "'TypeError: Not enough arguments'");

shouldBe("trySettingLineDashOffset(Infinity)", "initialLineDashOffset");
shouldBe("trySettingLineDashOffset(-Infinity)", "initialLineDashOffset");
shouldBe("trySettingLineDashOffset(NaN)", "initialLineDashOffset");
shouldBe("trySettingLineDashOffset('string')", "initialLineDashOffset");
