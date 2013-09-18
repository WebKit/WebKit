description(
'This test checks that setting a cyclic value for __proto__ throws an exception and does not alter __proto__.  This was reported as <a href="http://bugs.webkit.org/show_bug.cgi?id=17927">bug 17927</a>.'
);

x = {};
originalProto = x.__proto__;
shouldThrow('x.__proto__ = x;');
shouldBe("x.__proto__", "originalProto");
