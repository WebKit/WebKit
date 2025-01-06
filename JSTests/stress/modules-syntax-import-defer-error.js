//@ requireOptions("--useImportDefer=1")

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

checkModuleSyntaxError(`import defer "mod"`);
checkModuleSyntaxError(`import defer v from "mod"`);
checkModuleSyntaxError(`import defer, v from "mod"`);
checkModuleSyntaxError(`import defer, from "mod"`);
checkModuleSyntaxError(`import defer, as ns from "mod"`);
checkModuleSyntaxError(`import defer, defer from "mod"`);
checkModuleSyntaxError(`import defer, { e1, e2 }, * as ns from "mod"`);
checkModuleSyntaxError(`import defer, * as ns1, * as ns2 from "mod"`);
checkModuleSyntaxError(`import defer {} from "mod"`);
checkModuleSyntaxError(`import defer {x} from "mod"`);
checkModuleSyntaxError(`import defer {x,} from "mod"`);
checkModuleSyntaxError(`import defer {x, y} from "mod"`);
checkModuleSyntaxError(`import defer {x as v} from "mod"`);
checkModuleSyntaxError(`import defer {x}, y from "mod"`);
checkModuleSyntaxError(`import defer *`);
checkModuleSyntaxError(`import defer * as`);
checkModuleSyntaxError(`import defer * as {}`);
checkModuleSyntaxError(`import defer * as {} from "mod"`);
checkModuleSyntaxError(`import defer * as ns from`);
checkModuleSyntaxError(`import defer * ns from "mod"`);
checkModuleSyntaxError(`import defer * from "mod"`);
checkModuleSyntaxError(`import defer()`);
checkModuleSyntaxError(`export defer`);
checkModuleSyntaxError(`export defer * as ns from "mod"`);
