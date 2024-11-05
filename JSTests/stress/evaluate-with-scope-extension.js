//@ runBytecodeCacheNoAssertion

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

function makeTestCases(ident) {
    return [
        `${ident}()`,
        `"use strict"; ${ident}()`,
        `eval("${ident}()")`,
        `eval("'use strict'; ${ident}()")`,
        `(0, eval)("${ident}()")`,
        `(0, eval)("'use strict'; ${ident}()")`,
        `"use strict"; (0, eval)("${ident}()")`,
        `new Function("${ident}()")()`,
        `new Function("'use strict'; ${ident}()")()`,
        `new Function("eval('${ident}()')")()`,
        `new Function("'use strict'; eval('${ident}()')")()`,
        `eval('new Function("${ident}()")()')`,
        `"use strict"; eval('new Function("${ident}()")()')`,
        `(0, eval)('new Function("${ident}()")()')`,
        String.raw`eval('new Function("\'use strict\'; ${ident}()")()')`,
        String.raw`eval('new Function("\'use strict\'; eval(\'${ident}()\')")()')`,
        String.raw`(0, eval)('new Function("\'use strict\'; ${ident}()")()')`,
        String.raw`(0, eval)('new Function("\'use strict\'; eval(\'${ident}()\')")()')`,
    ];
}

function makeScopeObject() {
    function foo() { foo.lastThisValue = this; }
    function bar() { "use strict"; bar.lastThisValue = this; }

    return { foo, bar };
}

(function() {
    for (const testCase of makeTestCases("foo")) {
        const scopeExtension = makeScopeObject();

        for (let i = 0; i < 100; i++) {
            $vm.evaluateWithScopeExtension(testCase, scopeExtension);
            shouldBe(scopeExtension.foo.lastThisValue, scopeExtension);
        }

        shouldThrow(() => { $vm.evaluateWithScopeExtension(testCase); }, "ReferenceError: Can't find variable: foo");
    }

    for (const testCase of makeTestCases("bar")) {
        const scopeExtension = makeScopeObject();

        for (let i = 0; i < 100; i++) {
            $vm.evaluateWithScopeExtension(testCase, scopeExtension);
            shouldBe(scopeExtension.bar.lastThisValue, scopeExtension);
        }

        shouldThrow(() => { $vm.evaluateWithScopeExtension(testCase); }, "ReferenceError: Can't find variable: bar");
    }
})();
