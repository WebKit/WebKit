description(
'This test checks for a regression against <a href="rdar://problem/4161606">JavaScript regular expressions with certain ranges of Unicode characters cause a crash</a>. If it fails, it may crash.'
);


// test ranges reported in bug	
shouldBe('new RegExp("[\u00c0-\u1f4d]").toString()', '/[\u00c0-\u1f4d]/.toString()');
shouldBe('new RegExp("[\u3041-\u3094]").toString()', '/[\u3041-\u3094]/.toString()');
shouldBe('new RegExp("[\u4d00-\u4db5]").toString()', '/[\u4d00-\u4db5]/.toString()');
shouldBe('new RegExp("[\u4e00-\u9fa5]").toString()', '/[\u4e00-\u9fa5]/.toString()');

// test first char < 255, last char > 255
shouldBe('new RegExp("[\u0001-\u1f4d]").toString()', '/[\u0001-\u1f4d]/.toString()');
    
var successfullyParsed = true;
