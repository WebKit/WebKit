description("This test exercises the 'font' shorthand property in CSS computed styles.");

var testDiv = document.createElement('div');
document.body.appendChild(testDiv);

function computedFont(fontString) {
    testDiv.style.font = 'bold 600px serif';
    testDiv.style.font = fontString;
    return window.getComputedStyle(testDiv).getPropertyValue('font');
}

function computedFontCSSValue(fontString) {
    testDiv.style.font = 'bold 600px serif';
    testDiv.style.font = fontString;
    var cssValue = window.getComputedStyle(testDiv).getPropertyCSSValue('font');
    if (cssValue === null)
        return null;
    return cssValue.cssText;
}

shouldBe("computedFont('10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('10px SANS-SERIF')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('12px sans-serif')", "'normal normal normal 12px/normal sans-serif'");
shouldBe("computedFont('12px  sans-serif')", "'normal normal normal 12px/normal sans-serif'");
shouldBe("computedFont('10px sans-serif, sans-serif')", "'normal normal normal 10px/normal sans-serif, sans-serif'");
shouldBe("computedFont('10px sans-serif, serif')", "'normal normal normal 10px/normal sans-serif, serif'");
shouldBe("computedFont('12px ahem')", "'normal normal normal 12px/normal ahem'");
shouldBe("computedFont('12px unlikely-font-name')", "'normal normal normal 12px/normal unlikely-font-name'");
shouldBe("computedFont('100 10px sans-serif')", "'normal normal 100 10px/normal sans-serif'");
shouldBe("computedFont('200 10px sans-serif')", "'normal normal 200 10px/normal sans-serif'");
shouldBe("computedFont('300 10px sans-serif')", "'normal normal 300 10px/normal sans-serif'");
shouldBe("computedFont('400 10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('normal 10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('500 10px sans-serif')", "'normal normal 500 10px/normal sans-serif'");
shouldBe("computedFont('600 10px sans-serif')", "'normal normal 600 10px/normal sans-serif'");
shouldBe("computedFont('700 10px sans-serif')", "'normal normal bold 10px/normal sans-serif'");
shouldBe("computedFont('bold 10px sans-serif')", "'normal normal bold 10px/normal sans-serif'");
shouldBe("computedFont('800 10px sans-serif')", "'normal normal 800 10px/normal sans-serif'");
shouldBe("computedFont('900 10px sans-serif')", "'normal normal 900 10px/normal sans-serif'");
shouldBe("computedFont('italic 10px sans-serif')", "'italic normal normal 10px/normal sans-serif'");
shouldBe("computedFont('small-caps 10px sans-serif')", "'normal small-caps normal 10px/normal sans-serif'");
shouldBe("computedFont('italic small-caps 10px sans-serif')", "'italic small-caps normal 10px/normal sans-serif'");
shouldBe("computedFont('italic small-caps bold 10px sans-serif')", "'italic small-caps bold 10px/normal sans-serif'");
shouldBe("computedFont('10px/100% sans-serif')", "'normal normal normal 10px/10px sans-serif'");
shouldBe("computedFont('10px/100px sans-serif')", "'normal normal normal 10px/100px sans-serif'");
shouldBe("computedFont('10px/normal sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFont('10px/normal sans-serif')", "'normal normal normal 10px/normal sans-serif'");

shouldBe("computedFontCSSValue('10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('10px SANS-SERIF')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('12px sans-serif')", "'normal normal normal 12px/normal sans-serif'");
shouldBe("computedFontCSSValue('12px  sans-serif')", "'normal normal normal 12px/normal sans-serif'");
shouldBe("computedFontCSSValue('10px sans-serif, sans-serif')", "'normal normal normal 10px/normal sans-serif, sans-serif'");
shouldBe("computedFontCSSValue('10px sans-serif, serif')", "'normal normal normal 10px/normal sans-serif, serif'");
shouldBe("computedFontCSSValue('12px ahem')", "'normal normal normal 12px/normal ahem'");
shouldBe("computedFontCSSValue('12px unlikely-font-name')", "'normal normal normal 12px/normal unlikely-font-name'");
shouldBe("computedFontCSSValue('100 10px sans-serif')", "'normal normal 100 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('200 10px sans-serif')", "'normal normal 200 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('300 10px sans-serif')", "'normal normal 300 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('400 10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('normal 10px sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('500 10px sans-serif')", "'normal normal 500 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('600 10px sans-serif')", "'normal normal 600 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('700 10px sans-serif')", "'normal normal bold 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('bold 10px sans-serif')", "'normal normal bold 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('800 10px sans-serif')", "'normal normal 800 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('900 10px sans-serif')", "'normal normal 900 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('italic 10px sans-serif')", "'italic normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('small-caps 10px sans-serif')", "'normal small-caps normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('italic small-caps 10px sans-serif')", "'italic small-caps normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('italic small-caps bold 10px sans-serif')", "'italic small-caps bold 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('10px/100% sans-serif')", "'normal normal normal 10px/10px sans-serif'");
shouldBe("computedFontCSSValue('10px/100px sans-serif')", "'normal normal normal 10px/100px sans-serif'");
shouldBe("computedFontCSSValue('10px/normal sans-serif')", "'normal normal normal 10px/normal sans-serif'");
shouldBe("computedFontCSSValue('10px/normal sans-serif')", "'normal normal normal 10px/normal sans-serif'");
