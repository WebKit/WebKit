description(
"Make sure we don't crash when doing a put-direct-index beyond the vector length of a normal JSObject."
);

var o = {};
for (var i = 0; i < 100005; i += 3)
    Object.defineProperty(o, i, {enumerable:true, writable:true, configurable:true, value:"foo"});
shouldBe("o[0]", "\"foo\""); 
