description("Test that setting various CanvasRenderingContext2D properties to invalid values has no effect.");

ctx = document.createElement('canvas').getContext('2d');

function trySettingMiterLimit(value) {
    ctx.miterLimit = 1.5;
    ctx.miterLimit = value;
    return ctx.miterLimit;
}

function trySettingLineWidth(value) {
    ctx.lineWidth = 1.5;
    ctx.lineWidth = value;
    return ctx.lineWidth;
}

function trySettingShadowBlur(value) {
    ctx.shadowBlur = 1.5;
    ctx.shadowBlur = value;
    return ctx.shadowBlur;
}

function trySettingShadowOffsetX(value) {
    ctx.shadowOffsetX = 1.5;
    ctx.shadowOffsetX = value;
    return ctx.shadowOffsetX;
}

function trySettingShadowOffsetY(value) {
    ctx.shadowOffsetY = 1.5;
    ctx.shadowOffsetY = value;
    return ctx.shadowOffsetY;
}

shouldBe("trySettingMiterLimit(1)", "1");
shouldBe("trySettingMiterLimit(0)", "1.5");
shouldBe("trySettingMiterLimit(-1)", "1.5");
shouldBe("trySettingMiterLimit(Infinity)", "1.5");
shouldBe("trySettingMiterLimit(-Infinity)", "1.5");
shouldBe("trySettingMiterLimit(NaN)", "1.5");
shouldBe("trySettingMiterLimit('string')", "1.5");
shouldBe("trySettingMiterLimit(true)", "1");
shouldBe("trySettingMiterLimit(false)", "1.5");

shouldBe("trySettingLineWidth(1)", "1");
shouldBe("trySettingLineWidth(0)", "1.5");
shouldBe("trySettingLineWidth(-1)", "1.5");
shouldBe("trySettingLineWidth(Infinity)", "1.5");
shouldBe("trySettingLineWidth(-Infinity)", "1.5");
shouldBe("trySettingLineWidth(NaN)", "1.5");
shouldBe("trySettingLineWidth('string')", "1.5");
shouldBe("trySettingLineWidth(true)", "1");
shouldBe("trySettingLineWidth(false)", "1.5");

shouldBe("trySettingShadowBlur(1)", "1");
shouldBe("trySettingShadowBlur(0)", "0");
shouldBe("trySettingShadowBlur(-1)", "1.5");
shouldBe("trySettingShadowBlur(Infinity)", "1.5");
shouldBe("trySettingShadowBlur(-Infinity)", "1.5");
shouldBe("trySettingShadowBlur(NaN)", "1.5");
shouldBe("trySettingShadowBlur('string')", "1.5");
shouldBe("trySettingShadowBlur(true)", "1");
shouldBe("trySettingShadowBlur(false)", "0");

shouldBe("trySettingShadowOffsetX(1)", "1");
shouldBe("trySettingShadowOffsetX(0)", "0");
shouldBe("trySettingShadowOffsetX(-1)", "-1");
shouldBe("trySettingShadowOffsetX(Infinity)", "1.5");
shouldBe("trySettingShadowOffsetX(-Infinity)", "1.5");
shouldBe("trySettingShadowOffsetX(NaN)", "1.5");
shouldBe("trySettingShadowOffsetX('string')", "1.5");
shouldBe("trySettingShadowOffsetX(true)", "1");
shouldBe("trySettingShadowOffsetX(false)", "0");

shouldBe("trySettingShadowOffsetY(1)", "1");
shouldBe("trySettingShadowOffsetY(0)", "0");
shouldBe("trySettingShadowOffsetY(-1)", "-1");
shouldBe("trySettingShadowOffsetY(Infinity)", "1.5");
shouldBe("trySettingShadowOffsetY(-Infinity)", "1.5");
shouldBe("trySettingShadowOffsetY(NaN)", "1.5");
shouldBe("trySettingShadowOffsetY('string')", "1.5");
shouldBe("trySettingShadowOffsetY(true)", "1");
shouldBe("trySettingShadowOffsetY(false)", "0");
