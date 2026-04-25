importScripts("/resources/testharness.js");

test(() => {
    assert_false("onpush" in self);
}, "onpush should not be exposed on ServiceWorkerGlobalScope");

test(() => {
    assert_false("onpushsubscriptionchange" in self);
}, "onpushsubscriptionchange should not be exposed on ServiceWorkerGlobalScope");
