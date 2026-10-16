function shouldParse(source)
{
    try {
        new Function(source);
    } catch (error) {
        throw new Error(`Unexpected parse error for ${source}: ${error}`);
    }
}

function shouldFailToParse(source)
{
    try {
        new Function(source);
    } catch (error) {
        if (error instanceof SyntaxError)
            return;
        throw new Error(`Expected SyntaxError for ${source}, got ${error}`);
    }
    throw new Error(`Expected SyntaxError for ${source}`);
}

// The [~In] restriction on a for statement variable initializer does not
// propagate into syntactic contexts where Expression is parameterized with
// [+In] inside an arrow function body.
shouldParse('for (var f = () => ("x" in {}); false;) {}');
shouldParse('for (var f = () => { if ("x" in {}) {} }; false;) {}');
shouldParse('for (var f = () => { do {} while ("x" in {}); }; false;) {}');
shouldParse('for (var f = () => { while ("x" in {}) {} }; false;) {}');
shouldParse('for (var f = () => { for (var x in {}) {} }; false;) {}');
shouldParse('for (var f = () => { for (var g = () => { return "x" in {}; }; false;) {} }; false;) {}');
shouldParse('for (var f = () => { return "x" in {}; }; false;) {}');
shouldParse('for (var f = () => { with ("x" in {}) {} }; false;) {}');
shouldParse('for (var f = () => { switch ("x" in {}) { case "a" in {}: break; } }; false;) {}');
shouldParse('for (var f = () => { throw "x" in {}; }; false;) {}');

// ConciseBody inherits [~In] from the surrounding for initializer, so an
// unparenthesized `in` expression remains invalid here.
shouldFailToParse('for (var f = () => "x" in {}; false;) {}');
