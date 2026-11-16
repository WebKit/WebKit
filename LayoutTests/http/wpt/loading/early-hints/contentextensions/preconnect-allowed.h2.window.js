// META: script=/WebKit/loading/early-hints/resources/constants.sub.js

// Companion to preconnect-blocked.h2.window.js, guarding against the network-process-side rule list
// check being too strict. A rule list is installed here too, but none of its rules match the
// preconnect target, so the early hints preconnect must still be issued.
test(() => {
    const params = new URLSearchParams();
    params.set("preconnect_origin", `https://${CROSS_ORIGIN_HOST}`);

    window.location.replace(new URL("resources/preconnect.h2.py?" + params, window.location));
});
