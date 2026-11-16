function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

// IsIdentifierRef of a CoverParenthesizedExpressionAndArrowParameterList is false, so a
// parenthesized assignment target must not trigger NamedEvaluation of the right hand side.

(function () {
    var fn;
    (fn) = function () { };
    shouldBe(fn.name, "");

    var nested;
    (((nested))) = function () { };
    shouldBe(nested.name, "");

    var arrow;
    (arrow) = () => { };
    shouldBe(arrow.name, "");

    var klass;
    (klass) = class { };
    shouldBe(klass.name, "");

    var generator;
    (generator) = function* () { };
    shouldBe(generator.name, "");

    var asyncFn;
    (asyncFn) = async function () { };
    shouldBe(asyncFn.name, "");
}());

(function () {
    var coalesce;
    (coalesce) ??= function () { };
    shouldBe(coalesce.name, "");

    var or;
    (or) ||= function () { };
    shouldBe(or.name, "");

    var and = 1;
    (and) &&= function () { };
    shouldBe(and.name, "");
}());

(function () {
    var array;
    [(array) = function () { }] = [];
    shouldBe(array.name, "");

    var object;
    ({ p: (object) = function () { } } = {});
    shouldBe(object.name, "");
}());

(function () {
    var fn;
    fn = function () { };
    shouldBe(fn.name, "fn");

    var coalesce;
    coalesce ??= function () { };
    shouldBe(coalesce.name, "coalesce");

    var array;
    [array = function () { }] = [];
    shouldBe(array.name, "array");

    var object;
    ({ p: object = function () { } } = {});
    shouldBe(object.name, "object");

    let declaration = function () { };
    shouldBe(declaration.name, "declaration");
}());
