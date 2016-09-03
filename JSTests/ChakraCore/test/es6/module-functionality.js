//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Module functionality tests -- verifies functionality of import and export statements

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function testModuleScript(source, message, shouldFail) {
    let testfunc = () => WScript.LoadModule(source, 'samethread');

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
        name: "Validate a simple module export",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo } from 'ModuleSimpleExport.js';
                assert.areEqual('ModuleSimpleExport', ModuleSimpleExport_foo(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleExport.js');`;
            testModuleScript(functionBody, "Test importing a simple exported function", false);
        }
    },
    {
        name: "Validate importing from multiple modules",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo } from 'ModuleSimpleExport.js';
                assert.areEqual('ModuleSimpleExport', ModuleSimpleExport_foo(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleExport.js');
                import { foo2 } from 'ModuleComplexExports.js';
                assert.areEqual('foo', foo2(), 'Failed to import foo2 from ModuleComplexExports.js');`;
            testModuleScript(functionBody, "Test importing from multiple modules", false);
        }
    },
    {
        name: "Validate a variety of more complex exports",
        body: function () {
            let functionBody = 
                `import { foo, foo2 } from 'ModuleComplexExports.js';
                assert.areEqual('foo', foo(), 'Failed to import foo from ModuleComplexExports.js');
                assert.areEqual('foo', foo2(), 'Failed to import foo2 from ModuleComplexExports.js');
                import { bar, bar2 } from 'ModuleComplexExports.js';
                assert.areEqual('bar', bar(), 'Failed to import bar from ModuleComplexExports.js');
                assert.areEqual('bar', bar2(), 'Failed to import bar2 from ModuleComplexExports.js');
                import { let2, let3, let4, let5 } from 'ModuleComplexExports.js';
                assert.areEqual('let2', let2, 'Failed to import let2 from ModuleComplexExports.js');
                assert.areEqual('let3', let3, 'Failed to import let3 from ModuleComplexExports.js');
                assert.areEqual('let2', let4, 'Failed to import let4 from ModuleComplexExports.js');
                assert.areEqual('let3', let5, 'Failed to import let5 from ModuleComplexExports.js');
                import { const2, const3, const4, const5 } from 'ModuleComplexExports.js';
                assert.areEqual('const2', const2, 'Failed to import const2 from ModuleComplexExports.js');
                assert.areEqual('const3', const3, 'Failed to import const3 from ModuleComplexExports.js');
                assert.areEqual('const2', const4, 'Failed to import const4 from ModuleComplexExports.js');
                assert.areEqual('const3', const5, 'Failed to import const5 from ModuleComplexExports.js');
                import { var2, var3, var4, var5 } from 'ModuleComplexExports.js';
                assert.areEqual('var2', var2, 'Failed to import var2 from ModuleComplexExports.js');
                assert.areEqual('var3', var3, 'Failed to import var3 from ModuleComplexExports.js');
                assert.areEqual('var2', var4, 'Failed to import var4 from ModuleComplexExports.js');
                assert.areEqual('var3', var5, 'Failed to import var5 from ModuleComplexExports.js');
                import { class2, class3, class4, class5 } from 'ModuleComplexExports.js';
                assert.areEqual('class2', class2.static_member(), 'Failed to import class2 from ModuleComplexExports.js');
                assert.areEqual('class2', new class2().member(), 'Failed to create intance of class2 from ModuleComplexExports.js');
                assert.areEqual('class2', class3.static_member(), 'Failed to import class3 from ModuleComplexExports.js');
                assert.areEqual('class2', new class3().member(), 'Failed to create intance of class3 from ModuleComplexExports.js');
                assert.areEqual('class4', class4.static_member(), 'Failed to import class4 from ModuleComplexExports.js');
                assert.areEqual('class4', new class4().member(), 'Failed to create intance of class4 from ModuleComplexExports.js');
                assert.areEqual('class4', class5.static_member(), 'Failed to import class4 from ModuleComplexExports.js');
                assert.areEqual('class4', new class5().member(), 'Failed to create intance of class4 from ModuleComplexExports.js');
                import _default from 'ModuleComplexExports.js';
                assert.areEqual('default', _default(), 'Failed to import default from ModuleComplexExports.js');
                `;
            testModuleScript(functionBody, "Test importing a variety of exports", false);
        }
    },
    {
        name: "Import an export as a different binding identifier",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo as foo3 } from 'ModuleSimpleExport.js';
                assert.areEqual('ModuleSimpleExport', foo3(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleExport.js');
                import { foo2 as foo4 } from 'ModuleComplexExports.js';
                assert.areEqual('foo', foo4(), 'Failed to import foo4 from ModuleComplexExports.js');`;
            testModuleScript(functionBody, "Test importing as different binding identifiers", false);
        }
    },
    {
        name: "Import the same export under multiple local binding identifiers",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo as foo3, ModuleSimpleExport_foo as foo4 } from 'ModuleSimpleExport.js';
                assert.areEqual('ModuleSimpleExport', foo3(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleExport.js');
                assert.areEqual('ModuleSimpleExport', foo4(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleExport.js');
                assert.isTrue(foo3 === foo4, 'Export has the same value even if rebound');`;
            testModuleScript(functionBody, "Test importing the same export under multiple binding identifier", false);
        }
    },
    {
        name: "Exporting module changes exported value",
        body: function () {
            let functionBody = 
                `import { target, changeTarget } from 'ModuleComplexExports.js';
                assert.areEqual('before', target(), 'Failed to import target from ModuleComplexExports.js');
                assert.areEqual('ok', changeTarget(), 'Failed to import changeTarget from ModuleComplexExports.js');
                assert.areEqual('after', target(), 'changeTarget failed to change export value');`;
            testModuleScript(functionBody, "Changing exported value", false);
        }
    },
    {
        name: "Simple re-export forwards import to correct slot",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo } from 'ModuleSimpleReexport.js';
                assert.areEqual('ModuleSimpleExport', ModuleSimpleExport_foo(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleReexport.js');`;
            testModuleScript(functionBody, "Simple re-export from one module to another", false);
        }
    },
    {
        name: "Import of renamed re-export forwards import to correct slot",
        body: function () {
            let functionBody = 
                `import { ModuleSimpleExport_foo as ModuleSimpleExport_baz } from 'ModuleSimpleReexport.js';
                assert.areEqual('ModuleSimpleExport', ModuleSimpleExport_baz(), 'Failed to import ModuleSimpleExport_foo from ModuleSimpleReexport.js');`;
            testModuleScript(functionBody, "Rename simple re-export", false);
        }
    },
    {
        name: "Renamed re-export and renamed import",
        body: function () {
            let functionBody = 
                `import { ModuleComplexReexports_foo as ModuleComplexReexports_baz } from 'ModuleComplexReexports.js';
                assert.areEqual('bar', ModuleComplexReexports_baz(), 'Failed to import ModuleComplexReexports_foo from ModuleComplexReexports.js');`;
            testModuleScript(functionBody, "Rename already renamed re-export", false);
        }
    },
    {
        name: "Explicit export/import to default binding",
        body: function () {
            let functionBody = 
                `import { default as baz } from 'ModuleDefaultExport1.js';
                assert.areEqual('ModuleDefaultExport1', baz(), 'Failed to import default from ModuleDefaultExport1.js');`;
            testModuleScript(functionBody, "Explicitly export and import a local name to the default binding", false);
        }
    },
    {
        name: "Explicit import of default binding",
        body: function () {
            let functionBody = 
                `import { default as baz } from 'ModuleDefaultExport2.js';
                assert.areEqual('ModuleDefaultExport2', baz(), 'Failed to import default from ModuleDefaultExport2.js');`;
            testModuleScript(functionBody, "Explicitly import the default export binding", false);
        }
    },
    {
        name: "Implicitly re-export default export",
        body: function () {
            let functionBody = 
                `import baz from 'ModuleDefaultReexport.js';
                assert.areEqual('ModuleDefaultExport1', baz(), 'Failed to import default from ModuleDefaultReexport.js');`;
            testModuleScript(functionBody, "Implicitly re-export the default export binding", false);
        }
    },
    {
        name: "Implicitly re-export default export and rename the imported binding",
        body: function () {
            let functionBody = 
                `import { default as baz } from 'ModuleDefaultReexport.js';
                assert.areEqual('ModuleDefaultExport1', baz(), 'Failed to import default from ModuleDefaultReexport.js');
                import { not_default as bat } from 'ModuleDefaultReexport.js';
                assert.areEqual('ModuleDefaultExport2', bat(), 'Failed to import not_default from ModuleDefaultReexport.js');`;
            testModuleScript(functionBody, "Implicitly re-export the default export binding and rename the import binding", false);
        }
    },
    {
        name: "Exporting module changes value of default export",
        body: function () {
            let functionBody = 
                `import ModuleDefaultExport3_default from 'ModuleDefaultExport3.js';
                assert.areEqual(2, ModuleDefaultExport3_default, 'Failed to import default from ModuleDefaultExport3.js');
                import ModuleDefaultExport4_default from 'ModuleDefaultExport4.js';
                assert.areEqual(1, ModuleDefaultExport4_default, 'Failed to import not_default from ModuleDefaultExport4.js');`;
            testModuleScript(functionBody, "Exported value incorrectly bound", false);
        }
    },
    {
        name: "Import bindings used in a nested function",
        body: function () {
            let functionBody = 
                `function test() {
                    assert.areEqual('ModuleDefaultExport2', foo(), 'Failed to import default from ModuleDefaultExport2.js');
                }
                test();
                import foo from 'ModuleDefaultExport2.js';
                test();`;
            testModuleScript(functionBody, "Failed to find imported name correctly in nested function", false);
        }
    },
    {
        name: "Exported name may be any keyword",
        body: function () {
            let functionBody = 
                `import { export as baz } from 'ModuleComplexExports.js';
                assert.areEqual('ModuleComplexExports', baz, 'Failed to import export from ModuleDefaultExport2.js');
                import { function as bat } from 'ModuleComplexExports.js';
                assert.areEqual('ModuleComplexExports', bat, 'Failed to import function from ModuleDefaultExport2.js');`;
            testModuleScript(functionBody, "Exported name may be a keyword (import binding must be binding identifier)", false);
        }
    },
    {
        name: "Import binding of a keyword-named export may not be a keyword unless it is bound to a different binding identifier",
        body: function () {
            let functionBody = `import { export } from 'ModuleComplexExports.js';`;
            testModuleScript(functionBody, "Import binding must be binding identifier even if export name is not (export)", true);
            functionBody = `import { function } from 'ModuleComplexExports.js';`;
            testModuleScript(functionBody, "Import binding must be binding identifier even if export name is not (function)", true);
            functionBody = `import { switch } from 'ModuleComplexReexports.js';`;
            testModuleScript(functionBody, "Import binding must be binding identifier even if re-export name is not (switch)", true);
        }
    },
    {
        name: "Exported name may be any keyword testing re-exports",
        body: function () {
            let functionBody = 
                `import { switch as baz } from 'ModuleComplexReexports.js';
                assert.areEqual('ModuleComplexExports', baz, 'Failed to import switch from ModuleComplexReexports.js');`;
            testModuleScript(functionBody, "Exported name may be a keyword including re-epxort chains", false);
        }
    },
    {
        name: "Odd case of 'export { as as as }; import { as as as };'",
        body: function () {
            let functionBody = 
                `import { as as as } from 'ModuleComplexExports.js';
                assert.areEqual('as', as(), 'String "as" is not reserved word');`;
            testModuleScript(functionBody, "Test 'import { as as as}'", false);
        }
    },
    {
        name: "Typeof a module export",
        body: function () {
            let functionBody = 
                `import _default from 'ModuleDefaultExport2.js';
                assert.areEqual('function', typeof _default, 'typeof default export from ModuleDefaultExport2.js is function');`;
                
            WScript.LoadModule(functionBody, 'samethread');
        }
    },
    {
        name: "Circular module dependency",
        body: function () {
            let functionBody = 
                `import { circular_foo } from 'ModuleCircularFoo.js';
                assert.areEqual(2, circular_foo(), 'This function calls between both modules in the circular dependency incrementing a counter in each');
                import { circular_bar } from 'ModuleCircularBar.js';
                assert.areEqual(4, circular_bar(), 'Second call originates in the other module but still increments the counter twice');`;
                
            WScript.LoadModule(functionBody, 'samethread');
        }
    },
    {
        name: "Implicitly re-exporting an import binding (import { foo } from ''; export { foo };)",
        body: function () {
            let functionBody = 
                `import { foo, baz, localfoo, bar, localfoo2, bar2, bar2 as bar3 } from 'ModuleComplexReexports.js';
                assert.areEqual('foo', foo(), 'Simple implicit re-export');
                assert.areEqual('foo', baz(), 'Renamed export imported and renamed during implicit re-export');
                assert.areEqual('foo', localfoo(), 'Export renamed as import and implicitly re-exported');
                assert.areEqual('foo', bar(), 'Renamed export renamed as import and renamed again during implicit re-exported');
                assert.areEqual('foo', localfoo2(), 'Renamed export renamed as import and implicitly re-exported');
                assert.areEqual('foo', bar2(), 'Renamed export renamed as import and renamed again during implicit re-export');
                assert.areEqual('foo', bar3(), 'Renamed export renamed as import renamed during implicit re-export and renamed in final import');`;
                
            WScript.LoadModule(functionBody, 'samethread');
        }
    },
    {
        name: "Nested function in module function body which captures exported symbol doesn't create empty frame object",
        body: function() { 
            let functionBody = 
                `function foo() { };
                export { foo };
                function bar() { foo(); };`;
                
            WScript.LoadModule(functionBody, 'samethread');
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
