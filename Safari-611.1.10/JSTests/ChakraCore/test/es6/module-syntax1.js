//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Module syntax tests -- verifies syntax of import and export statements

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
        name: "All valid import statements",
        body: function () {
            assert.doesNotThrow(function () { WScript.LoadModuleFile('ValidImportStatements.js', 'samethread'); }, "Valid import statements");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
