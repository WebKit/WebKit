//@ requireOptions("--useSourcePhaseImports=1")

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (errorMessage) {
        if (String(error) !== errorMessage)
            throw new Error(`bad error: ${String(error)}`);
    }
}

function checkModuleSyntaxError(source, errorMessage) {
    shouldThrow(() => checkModuleSyntax(source), errorMessage);
}

checkModuleSyntaxError(`import source * as ns from "mod"`);
checkModuleSyntaxError(`import source { x } from "mod"`);
checkModuleSyntaxError(`import source x, * as ns from "mod"`);
checkModuleSyntaxError(`import source x, { y } from "mod"`);
checkModuleSyntaxError(`import source`);
checkModuleSyntaxError(`import source x`);
checkModuleSyntaxError(`export source x from "mod"`);
