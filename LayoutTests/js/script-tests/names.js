description(
"This tests an early experimental implementation of ES6-esque private names."
);

function forIn(o)
{
    var a = [];
    for (x in o)
        a.push(x);
    return a;
}

var prop = Name("prop");
var o = {};

shouldBeFalse("prop in o");
shouldBeFalse("'prop' in o");
shouldBe("Object.getOwnPropertyNames(o).length", '0');
shouldBe("forIn(o)", '[]');

o[prop] = 42;

shouldBeTrue("prop in o");
shouldBeFalse("'prop' in o");
shouldBe("Object.getOwnPropertyNames(o).length", '0');
shouldBe("forIn(o)", '[]');

o['prop'] = 101;

shouldBe("o[prop]", '42');
shouldBe("o['prop']", '101');
shouldBe("Object.getOwnPropertyNames(o).length", '1');
shouldBe("forIn(o)", '["prop"]');

delete o[prop];

shouldBeFalse("prop in o");
shouldBeTrue("'prop' in o");
shouldBe("Object.getOwnPropertyNames(o).length", '1');
shouldBe("forIn(o)", '["prop"]');

successfullyParsed = true;
