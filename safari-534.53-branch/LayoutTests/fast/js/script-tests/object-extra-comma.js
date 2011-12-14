description(
    'This test checks some object construction cases, including ' +
    '<a href="https://bugs.webkit.org/show_bug.cgi?id=5939">5939: final comma in javascript object prevents parsing</a>' +
    '.');

shouldBe("var foo = { 'bar' : 'YES' }; foo.bar", "'YES'");
shouldBe("var foo = { 'bar' : 'YES', }; foo.bar", "'YES'");
shouldBe("var foo = { 'bar' : 'YES' , }; foo.bar", "'YES'");
shouldThrow("var foo = { , 'bar' : 'YES' }; foo.bar");
shouldThrow("var foo = { 'bar' : 'YES',, }; foo.bar");

var successfullyParsed = true;
