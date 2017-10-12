import { shouldThrow } from "./resources/assert.js";

shouldThrow(() => {
    checkModuleSyntax(`import.meta = 42`);
}, `SyntaxError: import.meta can't be the left hand side of an assignment expression.:1`);

shouldThrow(() => {
    checkModuleSyntax(`import.meta += 42`);
}, `SyntaxError: import.meta can't be the left hand side of an assignment expression.:1`);

shouldThrow(() => {
    checkModuleSyntax(`++import.meta`);
}, `SyntaxError: import.meta can't come after a prefix operator.:1`);

shouldThrow(() => {
    checkModuleSyntax(`--import.meta`);
}, `SyntaxError: import.meta can't come after a prefix operator.:1`);

shouldThrow(() => {
    checkModuleSyntax(`import.meta++`);
}, `SyntaxError: import.meta can't come before a postfix operator.:1`);

shouldThrow(() => {
    checkModuleSyntax(`import.meta--`);
}, `SyntaxError: import.meta can't come before a postfix operator.:1`);
