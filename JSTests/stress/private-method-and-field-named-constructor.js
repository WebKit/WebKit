function assertSyntaxError(code) {
    try {
        eval(code);
        throw new Error("Should throw SyntaxError, but executed code without throwing");
    } catch(e) {
        if (!e instanceof SyntaxError)
            throw new Error("Should throw SyntaxError, but threw " + e);
    }
}

assertSyntaxError("let C = class { #constructor() {} }");
assertSyntaxError("let C = class { static #constructor() {} }");
assertSyntaxError("class C { #constructor() {} }");
assertSyntaxError("class C { static #constructor() {} }");

assertSyntaxError("let C = class { get #constructor() {} }");
assertSyntaxError("let C = class { static get #constructor() {} }");
assertSyntaxError("class C { get #constructor() {} }");
assertSyntaxError("class C { static get #constructor() {} }");

assertSyntaxError("let C = class { set #constructor(v) {} }");
assertSyntaxError("let C = class { static set #constructor(v) {} }");
assertSyntaxError("class C { set #constructor(v) {} }");
assertSyntaxError("class C { static set #constructor(v) {} }");

assertSyntaxError("let C = class { #constructor; }");
assertSyntaxError("let C = class { static #constructor; }");
assertSyntaxError("class C { #constructor; }");
assertSyntaxError("class C { static #constructor; }");

