// Global
var globalVariable = 1;

// GlobalLexicalEnvironment
let lexicalGlobalVariable = 2;

function testAllScopes() {
    // Closure
    var closureVariable1 = 3;
    let closureVariable2 = 4;

    function innerScope() {
        // Closure
        var innerClosureVariable1 = 5;
        let innerClosureVariable2 = 6;

        // With (the object)
        with ({withObjectProperty: 7, __proto__: null}) {
            // Block
            let withBlockVariable = 8;

            // Catch (the `e` object)
            try {
                throw 9;
            } catch (exceptionVariable) {
                // Block
                let catchBlockVariable = 10;

                // Function Name
                (function functionName() {

                    // Closure ("Local")
                    var localVariable1 = 11;
                    let localVariable2 = 12;

                    console.log("Variables",
                        globalVariable,
                        lexicalGlobalVariable,
                        closureVariable1, closureVariable2,
                        innerClosureVariable1, innerClosureVariable2,
                        withBlockVariable,
                        exceptionVariable,
                        catchBlockVariable,
                        functionName.name,
                        localVariable1, localVariable2);

                    // Pause.
                    debugger;
                })();
            }
        }
    }

    innerScope();
}
