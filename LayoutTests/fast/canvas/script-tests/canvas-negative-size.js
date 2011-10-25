description("Test that canvas elements can't have negative height and/or width.");

canvas = document.createElement('canvas');

function createFromMarkup(markup)
{
    var fragmentContainer = document.createElement("div");
    fragmentContainer.innerHTML = markup;
    return fragmentContainer.firstChild;
}

function trySettingWidth(value) {
    canvas.width = 6;
    canvas.width = value;
    return canvas.width;
}

function trySettingHeight(value) {
    canvas.height = 6;
    canvas.height = value;
    return canvas.height;
}

function trySettingWidthAttribute(value) {
    canvas.width = 6;
    canvas.setAttribute('width', value);
    return canvas.width;
}

function trySettingHeightAttribute(value) {
    canvas.height = 6;
    canvas.setAttribute('height', value);
    return canvas.height;
}

function tryCreatingCanvasWithWidth(value) {
    return createFromMarkup("<canvas width=" + value + "></canvas>").width;
}

function tryCreatingCanvasWithHeight(value) {
    return createFromMarkup("<canvas height='" + value + "'></canvas>").height;
}

function tryWidth(value, expected) {
    shouldBe("trySettingWidth(" + value + ")", expected);
    shouldBe("trySettingWidthAttribute(" + value + ")", expected);
    shouldBe("tryCreatingCanvasWithWidth(" + value + ")", expected);
}

function tryHeight(value, expected) {
    shouldBe("trySettingHeight(" + value + ")", expected);
    shouldBe("trySettingHeightAttribute(" + value + ")", expected);
    shouldBe("tryCreatingCanvasWithHeight(" + value + ")", expected);
}

function checkDefaultWidth() {
    return document.createElement("canvas").width;
}

function checkDefaultHeight() {
    return document.createElement("canvas").height;
}

shouldBe("checkDefaultWidth()", "300");
shouldBe("checkDefaultHeight()", "150");

shouldBe("trySettingWidth('abc')", "300");
shouldBe("trySettingWidth('200')", "200");
shouldBe("trySettingWidth('300')", "300");
shouldBe("trySettingWidth(NaN)", "300");
shouldBe("trySettingWidth(Infinity)", "300");
shouldBe("trySettingWidth(null)", "300");
shouldBe("trySettingWidth(true)", "1");
shouldBe("trySettingWidth(false)", "0");

shouldBe("trySettingHeight('abc')", "150");
shouldBe("trySettingHeight('200')", "200");
shouldBe("trySettingHeight('150')", "150");
shouldBe("trySettingHeight(NaN)", "150");
shouldBe("trySettingHeight(Infinity)", "150");
shouldBe("trySettingHeight(null)", "150");
shouldBe("trySettingHeight(true)", "1");
shouldBe("trySettingHeight(false)", "0");

tryWidth("'foo'", "300");
tryWidth("-1", "300");
tryWidth("0", "0");
tryWidth("1", "1");
tryWidth("'+7'", "7");
tryWidth("'-7'", "300");
tryWidth("'123'", "123");

tryHeight("'foo'", "150");
tryHeight("-1", "150");
tryHeight("0", "0");
tryHeight("1", "1");
tryHeight("'+7'", "7");
tryHeight("'-7'", "150");
tryHeight("'123'", "123");
