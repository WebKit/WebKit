import { shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-try-get.js"

function tryGetByIdText(propertyName) { return `(function (base) { return @tryGetById(base, '${propertyName}'); })`; }

function tryGetByIdTextStrict(propertyName) { return `(function (base) { "use strict"; return @tryGetById(base, '${propertyName}'); })`; }

{
    let get = createBuiltin(tryGetByIdText("empty"));
    noInline(get);

    // Do not throw.
    shouldBe(get(ns), null);

    let getStrict = createBuiltin(tryGetByIdTextStrict("empty"));
    noInline(getStrict);

    // Do not throw.
    shouldBe(getStrict(ns), null);

}

export let empty;
