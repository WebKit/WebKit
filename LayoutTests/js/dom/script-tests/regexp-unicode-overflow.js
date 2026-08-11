description(
'This test checks for a regression against <a href="rdar://problem/5271937"> REGRESSION: Apparent WebKit JavaScript memory smasher when submitting comment to iWeb site (crashes in kjs_pcre_compile2)</a>. If it fails, it may crash.'
);

//For some reason this needed to be done by a function call.
function createRegExs() {
    shouldBe('new RegExp("[k\xA0]", "i").toString()', '/[k\xA0]/i.toString()');
    shouldBe('new RegExp("[k\u2019]", "i").toString()', '/[k\u2019]/i.toString()');
    shouldBe('new RegExp("[k\u201c]", "i").toString()', '/[k\u201c]/i.toString()');    
    shouldBe('new RegExp("[k\u201d]", "i").toString()', '/[k\u201d]/i.toString()');
}

createRegExs();
