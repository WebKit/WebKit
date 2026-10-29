// META: script=/WebKit/loading/early-hints/resources/constants.sub.js

// This test's path contains "contentextensions/", so WebKitTestRunner compiles the sibling
// preconnect-blocked.h2.window.json and adds it to the page's WKUserContentController, exactly as
// Safari does for an installed content blocker. That rule list blocks ping loads to the cross-origin
// host, "ping" being the resource type content rule lists classify a preconnect as.
//
// 103 Early Hints are handled by EarlyHintsResourceLoader in the network process before any renderer
// has run, so the WebContent-side gate in LinkLoader::preconnectIfNeeded cannot see this ingress. If
// the network process does not consult the rule lists itself, the server-supplied preconnect target
// reaches the wire (DNS, TCP, and a TLS ClientHello naming the blocked host) despite the block rule.
test(() => {
    const params = new URLSearchParams();
    params.set("preconnect_origin", `https://${CROSS_ORIGIN_HOST}`);

    window.location.replace(new URL("resources/preconnect.h2.py?" + params, window.location));
});
