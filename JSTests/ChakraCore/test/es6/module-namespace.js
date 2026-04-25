//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Module syntax tests -- verifies syntax of import and export statements

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function testModuleScript(source, message, shouldFail) {
    let testfunc = () => WScript.LoadModule(source);

    if (shouldFail) {
        let caught = false;

        // We can't use assert.throws here because the SyntaxError used to construct the thrown error
        // is from a different context so it won't be strictly equal to our SyntaxError.
        try {
            testfunc();
        } catch(e) {
            caught = true;

            // Compare toString output of SyntaxError and other context SyntaxError constructor.
            assert.areEqual(e.constructor.toString(), SyntaxError.toString(), message);
        }

        assert.isTrue(caught, `Expected error not thrown: ${message}`);
    } else {
        assert.doesNotThrow(testfunc, message);
    }
}

var tests = [
    {
        name: "Basic import namespace",
        body: function () {
           testModuleScript('import * as foo from "ValidExportStatements.js"; for (var i in foo) helpers.writeln(i + "=" + foo[i]);', '', false);
       }
    },
    {
        name: "import namespace with verification",
        body: function () {
           testModuleScript('import * as foo from "moduleExport1.js"; for (var i in foo) WScript.Echo(i + "=" + foo[i]);', '', false);
           testModuleScript('import * as foo from "moduleExport1.js"; foo.verifyNamespace(foo);', '', false);
           testModuleScript('import * as foo from "moduleExport1.js"; foo.changeContext(); foo.verifyNamespace(foo);', '', false);
        }
    },
    {
        name: "reexport only",
        body: function () {
            testModuleScript('import * as foo from "ValidReExportStatements.js"; for (var i in foo) helpers.writeln(i + "=" + foo[i]);', '', false);
        }
    },
    {
        name: "complex reexport",
        body: function () {
           testModuleScript('import * as fooComplex from "ModuleComplexReexports.js"; for (var i in fooComplex) WScript.Echo(i + "=" + fooComplex[i]);', '', false);
        }
    },
    {
        name: "namespace as prototype",
        body: function () {
           testModuleScript('import * as foo from "ValidExportStatements.js"; var childObj = Object.create(foo); for (var i in childObj) helpers.writeln(i + "=" + childObj[i]);', '', false);
       }
    },
    {
        name: "namespace internal operations",
        body: function () {
            let functionBody = 
                `import * as foo from "ValidExportStatements.js";
                assert.areEqual(null, Object.getPrototypeOf(foo), 'namespace prototype is null');
                assert.areEqual(false, Object.isExtensible(foo), 'namespace is not extensible');
                assert.areEqual(false, Reflect.set(foo, "non_existing", 20), '[[set]] returns false ');
                assert.areEqual(undefined, foo.non_existing, 'namespace object is immutable');
                assert.areEqual(false, Reflect.set(foo, "4", 20), 'cannot set item in namespace obect');
                assert.areEqual(undefined, foo[4], 'cannot export item in namespace obect');
                assert.areEqual(false, Reflect.deleteProperty(foo, "var1"), 'cannot delete export in namespace obect');
                assert.areEqual(true, Reflect.deleteProperty(foo, "nonevar"), 'cannot delete export in namespace obect');
                assert.areEqual(undefined, foo[6], 'cannot get item in namespace obect');
                assert.areEqual(false, Reflect.set(foo, Symbol.species, 20), 'no species property');
                assert.areEqual(undefined, foo[Symbol.species], 'namespace is not contructor');
                assert.areEqual("module", foo[Symbol.toStringTag], 'namespace toStringTag');
                helpers.writeln("in iterator"); for (var i of foo) helpers.writeln(i);
                helpers.writeln("done with iterator")
                var symbols = Object.getOwnPropertySymbols(foo);
                assert.areEqual(2, symbols.length, "two symbols");
                assert.areEqual(symbols[0], Symbol.toStringTag, "first symbol is toStringTag");
                assert.areEqual(symbols[1], Symbol.iterator, "second symbol is iterator");
                assert.throws( function() {Object.setPrototypeOf(foo, Object)}, TypeError, 'Cannot create property for a non-extensible object');
                assert.areEqual(true, Reflect.preventExtensions(foo), '[[PreventExtensions]] for namespace object returns true');`;
            testModuleScript(functionBody, "Test importing as different binding identifiers", false);
       }
    },
   
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
