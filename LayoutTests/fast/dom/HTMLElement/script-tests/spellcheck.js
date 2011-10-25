description('Tests the spellcheck attribute.');

var parent = document.createElement("div");
document.body.appendChild(parent);

function testFor(initialAttribute, initialExpectation, setValue, lastExpectation, lastAttributeExpectation)
{
    var target = document.createElement("span");
    parent.appendChild(target);

    window.target = target;
    window.initialExpectation = initialExpectation;
    window.lastExpectation = lastExpectation;
    window.lastAttributeExpectation = lastAttributeExpectation;

    if (undefined !== initialAttribute)
        target.setAttribute("spellcheck", initialAttribute);
    shouldBe("target.spellcheck", "initialExpectation");
    
    if (undefined !== setValue)
        target.spellcheck = setValue;
    shouldBe("target.spellcheck", "lastExpectation");
    shouldBe("target.getAttribute('spellcheck')", "lastAttributeExpectation");

    parent.removeChild(target);
}

function testUsingSetAttributes()
{
    var target = document.createElement("span");
    parent.appendChild(target);
    window.target = target;

    shouldBe("target.spellcheck", "true");
    shouldBe("target.getAttribute('spellcheck')", "'true'");
    // Set using property.
    target.spellcheck = false;
    shouldBe("target.spellcheck", "false");
    shouldBe("target.getAttribute('spellcheck')", "'false'");
    // Set using setAttribute().
    target.setAttribute("spellcheck", "true");
    shouldBe("target.spellcheck", "true");
    shouldBe("target.getAttribute('spellcheck')", "'true'");

    // Set using setAttribute(), valid but non canonical value.
    target.spellcheck = false; // clear at first
    target.setAttribute("spellcheck", "TRUE");
    shouldBe("target.spellcheck", "true");
    shouldBe("target.getAttribute('spellcheck')", "'TRUE'");
    // Set using setAttribute(), invalid value.
    target.spellcheck = false; // clear at first
    target.setAttribute("spellcheck", "INVALID");
    shouldBe("target.spellcheck", "true");
    shouldBe("target.getAttribute('spellcheck')", "'INVALID'");

    parent.removeChild(target);
}

testFor(undefined, true, undefined, true, null);
testFor(undefined, true, false, false, "false");
testFor(undefined, true, true, true, "true");
testFor(undefined, true, 0, false, "false"); // 0 will be coerced to false
testFor(undefined, true, 1, true, "true"); // 0 will be coerced to true
testFor(undefined, true, "invalid", true, "true"); // string will be coerced to true
testFor(undefined, true, "false", true, "true"); // ...even if the string is "false" (as Firefox does). 

testFor("true", true, undefined, true, "true");
testFor("true", true, false, false, "false");
testFor("true", true, true, true, "true");
testFor("true", true, 0, false, "false");
testFor("true", true, 1, true, "true");
testFor("true", true, "invalid", true, "true");
testFor("true", true, "false", true, "true");

testFor("false", false, undefined, false, "false");
testFor("false", false, false, false, "false");
testFor("false", false, true, true, "true");
testFor("false", false, 0, false, "false");
testFor("false", false, 1, true, "true");
testFor("false", false, "invalid", true, "true");
testFor("false", false, "false", true, "true");

// various initial values
testFor("", true, undefined, true, "");
testFor("", true, 1, true, "true");
testFor("TRUE", true, undefined, true, "TRUE");
testFor("TRUE", true, 1, true, "true");
testFor("FALSE", false, undefined, false, "FALSE");
testFor("FALSE", false, 0, false, "false");
testFor("invalid", true, undefined, true, "invalid");
testFor("invalid", true, 1, true, "true");
testFor("false  ", true, undefined, true, "false  ");
testFor("false  ", true, 1, true, "true");
testFor("false  ", true, 0, false, "false");
testFor("0", true, undefined, true, "0");
testFor("0", true, 0, false, "false");
testFor("1", true, undefined, true, "1");
testFor("1", true, 0, false, "false");

testUsingSetAttributes();
