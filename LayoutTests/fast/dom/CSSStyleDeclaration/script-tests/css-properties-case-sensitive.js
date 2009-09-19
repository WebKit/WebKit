description(
'This test checks that access to CSS properties via JavaScript properties on DOM elements is case sensitive.'
);

var element = document.createElement('a');
element.style.zIndex = 1;

debug('normal cases');
debug('');

shouldBe("element.style.zIndex", "'1'");
shouldBeUndefined("element.style.ZIndex");

debug('');
debug('"css" prefix');
debug('');

shouldBe("element.style.cssZIndex", "'1'");
shouldBe("element.style.CssZIndex", "'1'");
shouldBeUndefined("element.style.CsszIndex");
shouldBeUndefined("element.style.csszIndex");

debug('');
debug('"pixel" prefix');
debug('');

shouldBe("element.style.pixelZIndex", "1");
shouldBe("element.style.PixelZIndex", "1");
shouldBeUndefined("element.style.pixelzIndex");
shouldBeUndefined("element.style.PixelzIndex");

debug('');
debug('"pos" prefix');
debug('');

shouldBe("element.style.posZIndex", "1");
shouldBe("element.style.PosZIndex", "1");
shouldBeUndefined("element.style.poszIndex");
shouldBeUndefined("element.style.PoszIndex");

var successfullyParsed = true;
