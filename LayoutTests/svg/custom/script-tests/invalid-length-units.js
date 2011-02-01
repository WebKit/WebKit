description("Tests handling of invalid SVG length units.");

var svgNS = "http://www.w3.org/2000/svg";

var svgRoot = document.createElementNS(svgNS, "svg");
document.documentElement.appendChild(svgRoot);

rect = document.createElementNS(svgNS, "rect");
svgRoot.appendChild(rect);

function trySettingLength(length, expected)
{
    rect.setAttribute('x', "1234");
    shouldBe("rect.setAttribute('x', '" + length + "'); rect.x.baseVal.valueAsString", expected);
}

trySettingLength("", "'0'");
trySettingLength(" ", "'0'");
trySettingLength("foo", "'0'");
trySettingLength("10foo", "'0'");
trySettingLength("px", "'0'");
trySettingLength("10px ", "'0'");
trySettingLength("10% ", "'0'");
trySettingLength("10 % ", "'0'");
trySettingLength("10 %", "'0'");
trySettingLength("10 px ", "'0'");
trySettingLength("10 px", "'0'");
trySettingLength("10", "'10'");
trySettingLength("10%", "'10%'");
trySettingLength("10em", "'10em'");
trySettingLength("10ex", "'10ex'");
trySettingLength("10px", "'10px'");
trySettingLength("10cm", "'10cm'");
trySettingLength("10mm", "'10mm'");
trySettingLength("10pt", "'10pt'");
trySettingLength("10pc", "'10pc'");

document.documentElement.removeChild(svgRoot);

var successfullyParsed = true;
