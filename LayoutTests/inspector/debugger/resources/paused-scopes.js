"use strict";

var globalVar1; // Global (globalVar1)
let globalLet2; // GlobalLexicalEnvironment (globalLet2)

function secondPause() { // Global (secondPause)
    let shoeLexicalVariable1 = document; // ClosureScope (shoeLexicalVariable1)
    if (true) {
        let blockLexicalVariable = "block"; // NestedBlockScope (blockLexicalVariable)
        debugger;
    }
}

function entry() { // Global (entry)
    var fooVarVariable1; // foo ClosureScope (fooVarVariable1)
    let fooLexicalVariable2; // foo ClosureScope (fooLexicalVariable2)
    firstPause();

    function firstPause() { // foo ClosureScope (firstPause)
        let fakeFirstPauseLexicalVariable; // firstPause ClosureScope (fakeFirstPauseLexicalVariable)
        (function firstPause() {
            var barVarVariable1 = window.navigator; // firstPause ClosureScope (barVarVariable1)
            let barLexicalVariable2 = window.navigator; // firstPause ClosureScope (barLexicalVariable2)
            if (true) {
                let barLexicalVariable2 = window.navigator; // NestedBlockScope (barLexicalVariable2)
                debugger;
                secondPause();
                thirdPause();
            }
        })();
    }
}

function thirdPause() {
    let closureVariableInsideThirdPause = false;
    (function() {
        let localVariableInsideAnonymousFunction = true;
        debugger;
    })();
}
