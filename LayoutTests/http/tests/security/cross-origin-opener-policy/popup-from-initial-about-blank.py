#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cross-Origin-Opener-Policy: same-origin-allow-popups\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<meta charset="utf-8">
<title>Navigation from a popup's initial about:blank document</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
const variants = [
    { expression: "window.open()", open: () => window.open() },
    { expression: "window.open('')", open: () => window.open("") },
    { expression: "window.open('about:blank')", open: () => window.open("about:blank") },
    { expression: "window.open('about:blank#foo')", open: () => window.open("about:blank#foo") },
    { expression: "window.open('about:blank?foo')", open: () => window.open("about:blank?foo") },
];

const resourcePath = "/security/cross-origin-opener-policy/resources/popup-from-initial-about-blank.py";

for (const variant of variants) {
    promise_test(async t => {
        const channelName = "popup-from-initial-about-blank-" + Math.random();
        const channel = new BroadcastChannel(channelName);
        const popup = variant.open();
        assert_not_equals(popup, null, `${variant.expression} returned a popup`);

        t.add_cleanup(() => {
            channel.postMessage("close");
            channel.close();
        });

        const destinationState = new Promise((resolve, reject) => {
            const timeout = t.step_timeout(() => reject(new Error("Timed out waiting for the popup destination")), 5000);
            channel.onmessage = event => {
                clearTimeout(timeout);
                resolve(event.data);
            };
        });

        popup.location = `${resourcePath}?channel=${encodeURIComponent(channelName)}`;

        const state = await destinationState;
        assert_true(state.hasOpener, "destination retained its opener");
        assert_false(popup.closed, "returned WindowProxy is not closed");
    }, `${variant.expression} preserves the initial popup relationship`);
}
</script>
''')
