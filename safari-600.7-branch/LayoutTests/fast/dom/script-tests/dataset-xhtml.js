description("This tests element.dataset for XHTML.");

function testGet(attr, expected)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");
    return d.dataset[expected] == "value";
}

shouldBeTrue("testGet('data-foo', 'foo')");
shouldBeTrue("testGet('data-foo-bar', 'fooBar')");
shouldBeTrue("testGet('data--', '-')");
shouldBeTrue("testGet('data--foo', 'Foo')");
shouldBeTrue("testGet('data---foo', '-Foo')");
shouldBeTrue("testGet('data-', '')");
shouldBeTrue("testGet('data-\xE0', '\xE0')");
debug("");

function matchesNothingInDataset(attr)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");

    var count = 0;
    for (var item in d.dataset)
        count++;
    return count == 0;
}

shouldBeTrue("matchesNothingInDataset('dataFoo')");
shouldBeTrue("matchesNothingInDataset('data-Foo')");
debug("");

function testSet(prop, expected)
{
    var d = document.createElement("div");
    d.dataset[prop] = "value";
    return d.getAttribute(expected) == "value";
}

shouldBeTrue("testSet('foo', 'data-foo')");
shouldBeTrue("testSet('fooBar', 'data-foo-bar')");
shouldBeTrue("testSet('-', 'data--')");
shouldBeTrue("testSet('Foo', 'data--foo')");
shouldBeTrue("testSet('-Foo', 'data---foo')");
shouldBeTrue("testSet('', 'data-')");
shouldBeTrue("testSet('\xE0', 'data-\xE0')");
debug("");

shouldThrow("testSet('-foo', 'dummy')", "'Error: SyntaxError: DOM Exception 12'");
shouldThrow("testSet('foo\x20', 'dummy')", "'Error: InvalidCharacterError: DOM Exception 5'");
shouldThrow("testSet('foo\uF900', 'dummy')", "'Error: InvalidCharacterError: DOM Exception 5'");
debug("");

function testDelete(attr, prop)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");
    delete d.dataset[prop];
    return d.getAttribute(attr) != "value";
}

shouldBeTrue("testDelete('data-foo', 'foo')");
shouldBeTrue("testDelete('data-foo-bar', 'fooBar')");
shouldBeTrue("testDelete('data--', '-')");
shouldBeTrue("testDelete('data--foo', 'Foo')");
shouldBeTrue("testDelete('data---foo', '-Foo')");
shouldBeTrue("testDelete('data-', '')");
shouldBeTrue("testDelete('data-\xE0', '\xE0')");
debug("");

shouldBeFalse("testDelete('dummy', '-foo')");
debug("");

function testForIn(array)
{
    var d = document.createElement("div");
    for (var i = 0; i < array.length; ++i) {
        d.setAttribute(array[i], "value");
    }

    var count = 0;
    for (var item in d.dataset)
        count++;

    return count;
}

shouldBe("testForIn(['data-foo', 'data-bar', 'data-baz'])", "3");
shouldBe("testForIn(['data-foo', 'data-bar', 'dataFoo'])", "2");
shouldBe("testForIn(['data-foo', 'data-bar', 'style'])", "2");
shouldBe("testForIn(['data-foo', 'data-bar', 'data-'])", "3");
