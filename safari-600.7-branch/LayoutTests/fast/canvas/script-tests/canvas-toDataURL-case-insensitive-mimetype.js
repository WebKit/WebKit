description("Test that toDataURL(mimeType) ignores the case of 'mimeType'.");

canvas = document.createElement('canvas');

function tryMimeType(mimeType) {
    re = new RegExp("^data:([^;,]+)[;,].*");
    data = canvas.toDataURL(mimeType);
    caps = data.match(re);
    if (caps.length != 2)
        return "FAIL";
    return caps[1];
}

shouldBe("tryMimeType('image/png')", "'image/png'");
shouldBe("tryMimeType('iMAge/Png')", "'image/png'");
shouldBe("tryMimeType('IMAGE/PNG')", "'image/png'");

if (tryMimeType("image/jpeg") == "image/jpeg") {
    shouldBe("tryMimeType('image/jpeg')", "'image/jpeg'");
    shouldBe("tryMimeType('imAgE/jPEg')", "'image/jpeg'");
    shouldBe("tryMimeType('IMAGE/JPEG')", "'image/jpeg'");
}
