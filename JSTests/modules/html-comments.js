import { shouldBe, shouldThrow } from "./resources/assert.js";

// Eval's goal symbol is Script, not Module.
shouldBe(eval(`
<!-- ok
--> ok
42
`), 42);

// Function's goal symbol is not Module.
shouldBe(new Function(`
<!-- ok
--> ok
return 42
`)(), 42);

shouldThrow(() => {
    checkModuleSyntax(`
    <!-- ng -->
    `)
}, `SyntaxError: Unexpected token '<':2`);

shouldThrow(() => {
    checkModuleSyntax(`
-->
    `)
}, `SyntaxError: Unexpected token '>':2`);

shouldThrow(() => {
    checkModuleSyntax(`
    function hello()
    {
        <!-- ng -->
    }
    `)
}, `SyntaxError: Unexpected token '<':4`);

shouldThrow(() => {
    checkModuleSyntax(`
    function hello()
    {
-->
    }
    `)
}, `SyntaxError: Unexpected token '>':4`);
