// META: script=/common/utils.js
// META: script=resources/constants.sub.js

// WebKit-specific: an Early Hints preload whose fetch is redirected cross-origin is abandoned (the
// speculative task has no NetworkLoadChecker), so the document's own, fully-checked request loads the
// resource instead. The resource still loads; it is just not served from the preload cache.
//
// Only the cross-origin (abandon) case is checked here: it is timing-independent because nothing is
// ever parked. The imported early-hints suite covers that preloads are served from the cache.

promise_test(async (t) => {
    const redirectId = token();
    const scriptId = token();

    // The preload's same-origin URL 302s to a cross-origin target.
    const target = new URL("resources/recorded-script.py?id=" + scriptId, location);
    target.host = CROSS_ORIGIN_HOST;
    const preloadPath = "redirect.py?id=" + redirectId + "&redirect_to=" + encodeURIComponent(target.href);

    const url = new URL("resources/preload-loader.h2.py", location);
    url.searchParams.set("preload_path", preloadPath);

    const win = window.open(url, "_blank");
    t.add_cleanup(() => win && win.close());

    const result = await new Promise((resolve) => {
        window.addEventListener("message", function onMessage(event) {
            if (event.source === win) {
                window.removeEventListener("message", onMessage);
                resolve(event.data);
            }
        });
    });

    assert_true(result.executed, "the resource still loaded via the document's own request");
    assert_false(result.preloadedByEarlyHints, "not served from the Early Hints preload cache");
}, "A cross-origin redirect while fetching an Early Hints preload abandons the preload");
