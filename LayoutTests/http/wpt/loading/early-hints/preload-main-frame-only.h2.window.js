// META: script=/common/utils.js

// WebKit-specific: Early Hints preloads are only processed for a top-level (main frame) navigation.
// A 103 preload delivered on a child navigable (iframe) navigation is ignored, so the iframe's own
// request loads the resource normally rather than from the preload cache. (The imported early-hints
// suite covers that preloads *are* served on a main-frame navigation.)

promise_test(async (t) => {
    const id = token();
    const url = new URL("resources/preload-loader.h2.py", location);
    url.searchParams.set("preload_path", "recorded-script.py?id=" + id);

    const iframe = document.createElement("iframe");
    iframe.src = url;
    document.body.appendChild(iframe);
    t.add_cleanup(() => iframe.remove());
    const win = iframe.contentWindow;

    const result = await new Promise((resolve) => {
        window.addEventListener("message", function onMessage(event) {
            if (event.source === win) {
                window.removeEventListener("message", onMessage);
                resolve(event.data);
            }
        });
    });

    assert_true(result.executed, "the resource loaded via the iframe's own request");
    assert_false(result.preloadedByEarlyHints, "not served from the Early Hints preload cache");
}, "An Early Hints preload delivered on a child navigable (iframe) navigation is ignored");
