import { shouldBe } from "./resources/assert.js"

Promise.all([
    import('./import-error/import-not-found.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Importing binding name 'B' is not found.`);
        }).catch($vm.abort),
    import('./import-error/import-ambiguous.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Importing binding name 'B' cannot be resolved due to ambiguous multiple bindings.`);
        }).catch($vm.abort),
    import('./import-error/import-default-from-star.js')
        .then($vm.abort, function (error) {
            shouldBe(String(error), `SyntaxError: Importing binding name 'default' cannot be resolved by star export entries.`);
        }).catch($vm.abort),
]).catch($vm.abort);
