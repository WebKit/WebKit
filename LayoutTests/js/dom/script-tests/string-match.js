description(
"String.match(&hellip;) test"
);

// match with a global regexp should set lastIndex to zero; if read-only this should throw.
// If the regexp is not global, lastIndex is not modified.
var re;
function testMatch(_re, readonly)
{
    re = _re;
    re.lastIndex = 3;
    if (readonly)
        re = Object.defineProperty(re, 'lastIndex', {writable:false});
    return '0x1x2'.match(re);
}

shouldBe("testMatch(/x/g, false)", '["x","x"]');
shouldThrow("testMatch(/x/g, true)");
shouldBe("testMatch(/x/, false)", '["x"]');
shouldBe("testMatch(/x/, true)", '["x"]');
shouldBe("testMatch(/x/g, false); re.lastIndex", '0');
shouldThrow("testMatch(/x/g, true); re.lastIndex");
shouldBe("testMatch(/x/, false); re.lastIndex", '3');
shouldBe("testMatch(/x/, true); re.lastIndex", '3');
