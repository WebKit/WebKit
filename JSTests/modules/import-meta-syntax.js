import { shouldThrow, shouldNotThrow } from "./resources/assert.js";

shouldThrow(() => {
    new Function(`import.meta`);
}, `SyntaxError: import.meta is only valid inside modules.`);

shouldNotThrow(() => {
    checkModuleSyntax(`import.meta`);
});

shouldThrow(() => {
    checkModuleSyntax(`(import.cocoa)`);
}, `SyntaxError: Unexpected identifier 'cocoa'. "import." can only be followed with meta.:1`);

shouldThrow(() => {
    checkModuleSyntax(`(import["Cocoa"])`);
}, `SyntaxError: Unexpected token '['. import call expects one or two arguments.:1`);

shouldThrow(() => {
    checkModuleSyntax(`import.cocoa`);
}, `SyntaxError: Unexpected identifier 'cocoa'. "import." can only be followed with meta.:1`);

shouldThrow(() => {
    checkModuleSyntax(`import["Cocoa"]`);
}, `SyntaxError: Unexpected token '['. Expected namespace import or import list.:1`);
