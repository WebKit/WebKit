function tag() {
}

function testSyntax(script) {
    try {
        eval(script);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Bad error: " + String(error));
    }
}

function testSyntaxError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected syntax error not thrown");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

testSyntax("tag``");
testSyntax("tag`Hello`");
testSyntax("tag`Hello${tag}`");
testSyntax("tag`${tag}`");
testSyntax("tag`${tag} ${tag}`");
testSyntax("tag`${tag}${tag}`");

testSyntax("tag.prop``");
testSyntax("tag.prop`Hello`");
testSyntax("tag.prop`Hello${tag}`");
testSyntax("tag.prop`${tag}`");
testSyntax("tag.prop`${tag} ${tag}`");
testSyntax("tag.prop`${tag}${tag}`");

testSyntax("tag[prop]``");
testSyntax("tag[prop]`Hello`");
testSyntax("tag[prop]`Hello${tag}`");
testSyntax("tag[prop]`${tag}`");
testSyntax("tag[prop]`${tag} ${tag}`");
testSyntax("tag[prop]`${tag}${tag}`");

testSyntax("(tag())``");
testSyntax("(tag())`Hello`");
testSyntax("(tag())`Hello${tag}`");
testSyntax("(tag())`${tag}`");
testSyntax("(tag())`${tag} ${tag}`");
testSyntax("(tag())`${tag}${tag}`");

testSyntax("(class { say() { super.tag`` } })");
testSyntax("(class { say() { super.tag`Hello` } })");
testSyntax("(class { say() { super.tag`Hello${tag}` } })");
testSyntax("(class { say() { super.tag`${tag}` } })");
testSyntax("(class { say() { super.tag`${tag} ${tag}` } })");
testSyntax("(class { say() { super.tag`${tag}${tag}` } })");

testSyntax("(class extends Hello { constructor() { super()`` } })");
testSyntax("(class extends Hello { constructor() { super()`Hello` } })");
testSyntax("(class extends Hello { constructor() { super()`Hello${tag}` } })");
testSyntax("(class extends Hello { constructor() { super()`${tag}` } })");
testSyntax("(class extends Hello { constructor() { super()`${tag} ${tag}` } })");
testSyntax("(class extends Hello { constructor() { super()`${tag}${tag}` } })");

testSyntaxError("super`Hello${tag}`", "SyntaxError: 'super' is only valid inside a function or an 'eval' inside a function.");
testSyntaxError("(class { say() { super`Hello${tag}` } })", "SyntaxError: Cannot use super as tag for tagged templates.");
