// META: script=/common/utils.js
// META: script=resources/constants.sub.js

// Security: an Early Hints preload parked during one site's navigation must not be served to a fetch
// made during a different site's (top-frame) navigation. The cache is scoped per-frame and keyed by
// cache partition, so a 103 from one domain can't respond to a fetch from another and leak content.

function openWindow(url) {
    const win = window.open(url, "_blank");
    add_completion_callback(() => win && win.close());
    return win;
}

function messageFrom(win) {
    return new Promise((resolve) => {
        window.addEventListener("message", function onMessage(event) {
            if (event.source === win) {
                window.removeEventListener("message", onMessage);
                resolve(event.data);
            }
        });
    });
}

promise_test(async () => {
    const id = token();
    // A same-origin resource on the first site (A), preloaded there and requested (cross-origin) from B.
    const sharedURL = new URL("resources/recorded-script.py?id=" + id, location).href;

    // Site A: park a preload for `sharedURL` during A's navigation, but don't consume it here. A
    // reports once the preload has actually completed, so B below runs against a real parked entry
    // rather than passing vacuously because nothing was ever stored.
    const a = new URL("resources/preload-loader.h2.py", location);
    a.searchParams.set("preload_path", "recorded-script.py?id=" + id);
    a.searchParams.set("wait_key", id);
    a.searchParams.set("no_consume", "1");
    const parked = await messageFrom(openWindow(a));
    assert_true(parked.parked, "the first site's Early Hints preload completed and was parked");

    // Site B (a different registrable domain): fetch the same URL A preloaded. It must load via B's own
    // request, not be served A's parked preload.
    const b = new URL("resources/preload-loader.h2.py", location);
    b.host = CROSS_ORIGIN_HOST;
    b.searchParams.set("preload_path", "recorded-script.py?id=" + token()); // B doesn't preload sharedURL
    b.searchParams.set("fetch_url", sharedURL);
    const result = await messageFrom(openWindow(b));

    assert_true(result.executed, "the resource still loaded via the cross-site window's own request");
    assert_false(result.preloadedByEarlyHints, "one site's Early Hints preload is not served to another site");
}, "An Early Hints preload from one site is not served to a fetch from a different site");
