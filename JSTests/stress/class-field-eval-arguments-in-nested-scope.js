// https://tc39.es/ecma262/#sec-performeval-rules-in-initializer
// It is a Syntax Error if ContainsArguments of |StatementList| is true.
//
// Two bugs:
// (A) Inside the eval script, entering a nested lexical / arrow scope lost the
//     InstanceFieldEvalContext flag, so `eval("{ arguments; }")` slipped through.
// (B) When the eval call is inside an arrow function nested in the initializer,
//     the caller's EvalContextType was not propagated through the arrow, so
//     `x = () => eval("arguments")` slipped through.

function shouldThrowSyntaxError(name, fn) {
    var errorName = null;
    try {
        fn();
    } catch (e) {
        errorName = e.constructor.name;
    }
    if (errorName !== "SyntaxError")
        throw new Error(name + ": expected SyntaxError, got " + errorName);
}

function shouldNotThrowSyntaxError(name, fn) {
    var errorName = null;
    try {
        fn();
    } catch (e) {
        errorName = e.constructor.name;
    }
    if (errorName === "SyntaxError")
        throw new Error(name + ": unexpected SyntaxError");
}

// ===== Baseline (must keep working) =====

shouldThrowSyntaxError("direct-bare", () => {
    class C { x = eval("arguments"); }
    new C();
});

shouldNotThrowSyntaxError("in-function", () => {
    class C { x = eval("(function() { arguments; })"); }
    new C();
});

shouldNotThrowSyntaxError("in-generator", () => {
    class C { x = eval("(function*() { arguments; })"); }
    new C();
});

shouldNotThrowSyntaxError("in-async-function", () => {
    class C { x = eval("(async function() { arguments; })"); }
    new C();
});

shouldNotThrowSyntaxError("in-method", () => {
    class C { x = eval("({ m() { arguments; } })"); }
    new C();
});

shouldNotThrowSyntaxError("outer-fn-between", () => {
    // Regular function between initializer and eval breaks the link.
    class C { x = () => (function() { return eval("arguments"); })(1, 2); }
    new C().x();
});

// ===== Bug A: eval script loses context in nested scopes =====

shouldThrowSyntaxError("A:block", () => {
    class C { x = eval("{ arguments; }"); }
    new C();
});

shouldThrowSyntaxError("A:if-block", () => {
    class C { x = eval("if (true) { arguments; }"); }
    new C();
});

shouldThrowSyntaxError("A:for", () => {
    class C { x = eval("for (let i = 0; i < 1; i++) arguments;"); }
    new C();
});

shouldThrowSyntaxError("A:arrow", () => {
    class C { x = eval("() => arguments"); }
    new C();
});

shouldThrowSyntaxError("A:arrow-iife", () => {
    class C { x = eval("(() => arguments)()"); }
    new C();
});

shouldThrowSyntaxError("A:nested-arrow", () => {
    class C { x = eval("() => () => arguments"); }
    new C();
});

shouldThrowSyntaxError("A:async-arrow", () => {
    class C { x = eval("async () => arguments"); }
    new C();
});

// ===== Bug B: arrow between initializer and eval call =====

shouldThrowSyntaxError("B:arrow-eval", () => {
    class C { x = () => eval("arguments"); }
    new C().x();
});

shouldThrowSyntaxError("B:nested-arrow", () => {
    class C { x = () => (() => eval("arguments"))(); }
    new C().x();
});

shouldThrowSyntaxError("B:static", () => {
    class C { static x = () => eval("arguments"); }
    C.x();
});

shouldThrowSyntaxError("B:private", () => {
    class C {
        #x = () => eval("arguments");
        run() { return this.#x(); }
    }
    new C().run();
});

shouldThrowSyntaxError("B:class-expr", () => {
    let C = class { x = () => eval("arguments"); };
    new C().x();
});

// ===== A + B combined =====

shouldThrowSyntaxError("AB:arrow-then-block", () => {
    class C { x = () => eval("{ arguments; }"); }
    new C().x();
});

shouldThrowSyntaxError("AB:arrow-then-arrow", () => {
    class C { x = () => eval("() => arguments"); }
    new C().x();
});
