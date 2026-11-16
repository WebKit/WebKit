description("Tests that ToString on a cell works.");

function foo(a, o) {
    return [a, String(o)];
}

for (var i = 0; i < 100; ++i)
    shouldBe("\"\" + foo(\"foo\", i % 2 ? \"hello\" : [1, 2, 3])", i % 2 ? "\"foo,hello\"" : "\"foo,1,2,3\"");

