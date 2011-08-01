description(
"This tests the HTMLImageElement border property."
);

function imageBorderWidth(borderValue, style)
{
    var image = document.createElement("img");
    if (borderValue !== undefined)
        image.setAttribute("border", borderValue);
    image.setAttribute("style", style);
    image.setAttribute("width", "0");
    document.body.appendChild(image);
    var borderBoxWidth = image.offsetWidth;
    document.body.removeChild(image);
    return borderBoxWidth / 2;
}

shouldBe("imageBorderWidth()", "0");
shouldBe("imageBorderWidth(null)", "0");
shouldBe("imageBorderWidth('')", "0");
shouldBe("imageBorderWidth(0)", "0");
shouldBe("imageBorderWidth('x')", "0");
shouldBe("imageBorderWidth(undefined, 'border-width: 20px')", "0");

shouldBe("imageBorderWidth(null, 'border-width: 20px')", "20");
shouldBe("imageBorderWidth('', 'border-width: 20px')", "20");
shouldBe("imageBorderWidth('x', 'border-width: 20px')", "20");
shouldBe("imageBorderWidth(0, 'border-width: 20px')", "20");

shouldBe("imageBorderWidth(10)", "10");
shouldBe("imageBorderWidth(' 10')", "10");
shouldBe("imageBorderWidth('10 ')", "10");
shouldBe("imageBorderWidth(' 10 ')", "10");
shouldBe("imageBorderWidth('10q')", "10");
shouldBe("imageBorderWidth(' 10q')", "10");
shouldBe("imageBorderWidth('10q ')", "10");
shouldBe("imageBorderWidth(' 10q ')", "10");

var successfullyParsed = true;
