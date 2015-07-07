description('This test exercises the source URL and line number that is embedded in JavaScript exceptions, which is displayed in places like the JavaScript console.');

function exceptionInFunction()
{
    throw Exception();
}

var e = undefined;

try {
    // Raises an exception that gets picked up by KJS_CHECKEXCEPTION
    document.documentElement.innerHTML(foo);
} catch (exception) {
    e = exception;
}
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '12');

e = undefined;
try {
    // Raises an exception that gets picked up by KJS_CHECKEXCEPTIONVALUE
    document.documentElement.appendChild('').prefix = '';
} catch (exception) {
    e = exception;
}
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '22');

e = undefined;
try {
    // Raises an exception that gets picked up by KJS_CHECKEXCEPTIONREFERENCE
    document.documentElement.appendChild('').innerHTML = '';
} catch (exception) {
    e = exception;
}
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '32');

e = undefined;
try {
    // Raises an exception that gets picked up by KJS_CHECKEXCEPTIONLIST
    document.getElementById(1());
} catch (exception) {
    e = exception;
}
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '42');

e = undefined;
// Raises an exception inside a function to check that its line number
// isn't overwritten in the assignment node.
try {
    var a = exceptionInFunction();
} catch (exception) {
    e = exception;
}
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '5');

realEval = eval;
delete eval;
(function(){
    try {
        eval("");
    } catch(exception) {
        e = exception;
    }
})();
eval = realEval;
shouldBe("typeof e.sourceURL", '"string"');
shouldBe("e.line", '64');

var firstPropIsGetter = {
    get getter() { throw {} }
};
var secondPropIsGetter = {
    prop: 1,
    get getter() { throw {} }
};
var firstPropIsSetter = {
    set setter(a) { throw {} }
};
var secondPropIsSetter = {
    prop: 1,
    set setter(a) { throw {} }
};

try {
    firstPropIsGetter.getter;
} catch(ex) {
    e = ex;
    shouldBe("e.line", "74");
}

try {
    secondPropIsGetter.getter;
} catch(ex) {
    e = ex;
    shouldBe("e.line", "78");
}

try {
    firstPropIsSetter.setter = '';
} catch(ex) {
    e = ex;
    shouldBe("e.line", "81");
}

try {
    secondPropIsSetter.setter = '';
} catch(ex) {
    e = ex;
    shouldBe("e.line", "85");
}
