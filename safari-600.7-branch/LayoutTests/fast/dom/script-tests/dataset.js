description("This tests element.dataset.");

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
shouldBeTrue("testGet('data---foo--bar', '-Foo-Bar')");
shouldBeTrue("testGet('data---foo---bar', '-Foo--Bar')");
shouldBeTrue("testGet('data-foo-', 'foo-')");
shouldBeTrue("testGet('data-foo--', 'foo--')");
shouldBeTrue("testGet('data-Foo', 'foo')"); // HTML lowercases all attributes.
shouldBeTrue("testGet('data-', '')");
shouldBeTrue("testGet('data-\xE0', '\xE0')");
shouldBeUndefined("document.body.dataset.nonExisting");
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


debug("");
debug("Property override:");
var div = document.createElement("div");
// If the Object prorotype already has "foo", dataset doesn't create the
// corresponding attribute for "foo".
shouldBe("Object.prototype.foo = 'on Object'; div.dataset.foo", "'on Object'");
shouldBe("div.dataset['foo'] = 'on dataset'; div.dataset.foo", "'on dataset'");
shouldBeTrue("div.hasAttribute('data-foo')");
shouldBe("div.setAttribute('data-foo', 'attr'); div.dataset.foo", "'attr'");
debug("Update the JavaScript property:");
shouldBe("div.dataset.foo = 'updated'; div.dataset.foo", "'updated'");
shouldBe("div.getAttribute('data-foo')", "'updated'");
// "Bar" can't be represented as a data- attribute.
shouldBe("div.dataset.Bar = 'on dataset'; div.dataset.Bar", "'on dataset'");
shouldBeFalse("div.hasAttribute('data-Bar')");
debug("Make the JavaScript property empty:");
shouldBe("div.dataset.foo = ''; div.dataset.foo", "''");
shouldBe("div.getAttribute('data-foo')", "''");
debug("Remove the attribute:");
shouldBe("div.removeAttribute('data-foo'); div.dataset.foo", "'on Object'");
debug("Remove the JavaScript property:");
shouldBe("div.setAttribute('data-foo', 'attr'); delete div.dataset.foo; div.dataset.foo", "'on Object'");
shouldBeFalse("div.hasAttribute('foo')");
shouldBeUndefined("delete div.dataset.Bar; div.dataset.Bar");

debug("");
