// META: script=/WebKit/loading/early-hints/resources/constants.sub.js

// Companion to preconnect-blocked.h2.window.js that redirects the main-frame navigation BEFORE the
// 103. The initial request (redirect_from=1) 301-redirects to the same script without that param;
// the 103 Early Hints preconnect then arrives on the post-redirect response. The network-process
// content-rule-list check must treat the post-redirect document URL (response.url()) as the
// main-document URL, matching the renderer-side path in
// ContentExtensionsBackend::processContentRuleListsForLoad -- NOT the loader's original,
// pre-redirect request URL. The sibling preconnect-redirect.h2.window.json blocks ping loads to the
// cross-origin host, and the console message EarlyHintsResourceLoader emits names the main-document
// URL it used, so the expectations file pins which URL the check consulted.
test(() => {
    const params = new URLSearchParams();
    params.set("preconnect_origin", `https://${CROSS_ORIGIN_HOST}`);
    params.set("redirect_from", "1");

    window.location.replace(new URL("resources/preconnect.h2.py?" + params, window.location));
});
