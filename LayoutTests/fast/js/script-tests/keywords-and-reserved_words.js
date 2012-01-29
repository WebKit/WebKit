description(
"This test verifies that keywords and reserved words match those specified in ES5 section 7.6."
);

function isKeyword(x)
{
    try {
        eval("var "+x+";");
    } catch(e) {
        return true;
    }
    
    return false;
}

function isStrictKeyword(x)
{
    try {
        eval("'use strict'; var "+x+";");
    } catch(e) {
        return true;
    }
    
    return false;
}

function classifyIdentifier(x)
{
    if (isKeyword(x)) {
        // All non-strict keywords are also keywords in strict code.
        if (!isStrictKeyword(x))
            return "ERROR";
        return "keyword";
    }

    // Check for strict mode future reserved words.
    if (isStrictKeyword(x))
        return "strict";

    return "identifier";
}

// Not keywords - these are all just identifiers.
shouldBe('classifyIdentifier("x")', '"identifier"');
shouldBe('classifyIdentifier("id")', '"identifier"');
shouldBe('classifyIdentifier("identifier")', '"identifier"');
shouldBe('classifyIdentifier("keyword")', '"identifier"');
shouldBe('classifyIdentifier("strict")', '"identifier"');
shouldBe('classifyIdentifier("use")', '"identifier"');
// These are identifiers that we used to treat as keywords!
shouldBe('classifyIdentifier("abstract")', '"identifier"');
shouldBe('classifyIdentifier("boolean")', '"identifier"');
shouldBe('classifyIdentifier("byte")', '"identifier"');
shouldBe('classifyIdentifier("char")', '"identifier"');
shouldBe('classifyIdentifier("double")', '"identifier"');
shouldBe('classifyIdentifier("final")', '"identifier"');
shouldBe('classifyIdentifier("float")', '"identifier"');
shouldBe('classifyIdentifier("goto")', '"identifier"');
shouldBe('classifyIdentifier("int")', '"identifier"');
shouldBe('classifyIdentifier("long")', '"identifier"');
shouldBe('classifyIdentifier("native")', '"identifier"');
shouldBe('classifyIdentifier("short")', '"identifier"');
shouldBe('classifyIdentifier("synchronized")', '"identifier"');
shouldBe('classifyIdentifier("throws")', '"identifier"');
shouldBe('classifyIdentifier("transient")', '"identifier"');
shouldBe('classifyIdentifier("volatile")', '"identifier"');

// Keywords.
shouldBe('classifyIdentifier("break")', '"keyword"');
shouldBe('classifyIdentifier("case")', '"keyword"');
shouldBe('classifyIdentifier("catch")', '"keyword"');
shouldBe('classifyIdentifier("continue")', '"keyword"');
shouldBe('classifyIdentifier("debugger")', '"keyword"');
shouldBe('classifyIdentifier("default")', '"keyword"');
shouldBe('classifyIdentifier("delete")', '"keyword"');
shouldBe('classifyIdentifier("do")', '"keyword"');
shouldBe('classifyIdentifier("else")', '"keyword"');
shouldBe('classifyIdentifier("finally")', '"keyword"');
shouldBe('classifyIdentifier("for")', '"keyword"');
shouldBe('classifyIdentifier("function")', '"keyword"');
shouldBe('classifyIdentifier("if")', '"keyword"');
shouldBe('classifyIdentifier("in")', '"keyword"');
shouldBe('classifyIdentifier("instanceof")', '"keyword"');
shouldBe('classifyIdentifier("new")', '"keyword"');
shouldBe('classifyIdentifier("return")', '"keyword"');
shouldBe('classifyIdentifier("switch")', '"keyword"');
shouldBe('classifyIdentifier("this")', '"keyword"');
shouldBe('classifyIdentifier("throw")', '"keyword"');
shouldBe('classifyIdentifier("try")', '"keyword"');
shouldBe('classifyIdentifier("typeof")', '"keyword"');
shouldBe('classifyIdentifier("var")', '"keyword"');
shouldBe('classifyIdentifier("void")', '"keyword"');
shouldBe('classifyIdentifier("while")', '"keyword"');
shouldBe('classifyIdentifier("with")', '"keyword"');
// Technically these are "Future Reserved Words"!
shouldBe('classifyIdentifier("class")', '"keyword"');
shouldBe('classifyIdentifier("const")', '"keyword"');
shouldBe('classifyIdentifier("enum")', '"keyword"');
shouldBe('classifyIdentifier("export")', '"keyword"');
shouldBe('classifyIdentifier("extends")', '"keyword"');
shouldBe('classifyIdentifier("import")', '"keyword"');
shouldBe('classifyIdentifier("super")', '"keyword"');

// Future Reserved Words, in strict mode only.
shouldBe('classifyIdentifier("implements")', '"strict"');
shouldBe('classifyIdentifier("interface")', '"strict"');
shouldBe('classifyIdentifier("let")', '"keyword"'); // Experimentally reserving this; this may be reserved in ES6.
shouldBe('classifyIdentifier("package")', '"strict"');
shouldBe('classifyIdentifier("private")', '"strict"');
shouldBe('classifyIdentifier("protected")', '"strict"');
shouldBe('classifyIdentifier("public")', '"strict"');
shouldBe('classifyIdentifier("static")', '"strict"');
shouldBe('classifyIdentifier("yield")', '"strict"');
