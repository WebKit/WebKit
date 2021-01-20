import { shouldBe } from "./resources/assert.js";
import * as A from "./namespace-object-inline-caching/a.js"
import * as B from "./namespace-object-inline-caching/b.js"

// unset caching should be disabled for namespace object.
{
    function lookup(ns)
    {
        return ns.hello;
    }
    noInline(lookup);

    shouldBe(A.hello, undefined);
    shouldBe(B.hello, 42);

    for (let i = 0; i < 1e4; ++i)
        shouldBe(lookup(A), undefined);

    shouldBe(lookup(B), 42);
}

// usual caching should be disabled for namespace object.
{
    function lookup(ns)
    {
        return ns.goodbye;
    }
    noInline(lookup);

    shouldBe(A.goodbye, 0);
    shouldBe(B.goodbye, undefined);

    for (let i = 0; i < 1e4; ++i)
        shouldBe(lookup(A), 0);

    shouldBe(lookup(B), undefined);
}
