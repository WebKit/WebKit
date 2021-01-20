
function shouldBeSyntaxError(s) {
    let isSyntaxError = false;
    try {
        eval(s);
    } catch(e) {
        if (e instanceof SyntaxError)
            isSyntaxError = true;
    }
    if (!isSyntaxError)
        throw new Error("expected a syntax error");
}
noInline(shouldBeSyntaxError);

function shouldNotBeSyntaxError(s) {
    let isSyntaxError = false;
    try {
        eval(s);
    } catch(e) {
        if (e instanceof SyntaxError)
            isSyntaxError = true;
    }
    if (isSyntaxError)
        throw new Error("did not expect a syntax error");
}

function truth() { return true; }
noInline(truth);

shouldBeSyntaxError("class A { }; class A { };");
shouldBeSyntaxError("function foo() { class A { }; class A { }; }");
shouldBeSyntaxError("function foo() { if (truth()) { class A { }; class A { }; } }");
shouldBeSyntaxError("switch(10) { case 10: class A { }; break; case 20: class A { } }");
shouldBeSyntaxError("if (truth()) class A { }");
shouldNotBeSyntaxError("switch(10) { case 10: { class A { }; break; } case 20: class A { } }");
shouldNotBeSyntaxError("class A { } if (truth()) { class A { } }");
