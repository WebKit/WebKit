import { shouldBe } from "./resources/assert.js"


Promise.all([
    import('./indirect-export-error/indirect-export-not-found.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Indirectly exported binding name 'B' is not found.`);
        }).catch($vm.abort),
    import('./indirect-export-error/indirect-export-ambiguous.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Indirectly exported binding name 'B' cannot be resolved due to ambiguous multiple bindings.`);
        }).catch($vm.abort),
    import('./indirect-export-error/indirect-export-default.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Indirectly exported binding name 'default' cannot be resolved by star export entries.`);
        }).catch($vm.abort),
]).catch($vm.abort);
