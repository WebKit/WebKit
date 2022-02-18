importScripts("/resources/testharness.js");

test(() => {
    assert_true("onpush" in self);
}, "onpush should be exposed on ServiceWorkerGlobalScope");

test(() => {
    assert_true("onpushsubscriptionchange" in self);
}, "onpushsubscriptionchange should be exposed on ServiceWorkerGlobalScope");
