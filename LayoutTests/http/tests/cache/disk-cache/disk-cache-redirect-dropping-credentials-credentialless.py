#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cross-Origin-Embedder-Policy: credentialless\r\n\r\n'
)

print('''<!DOCTYPE html>
<title>A redirect from the disk cache that stops credentials from being sent in a credentialless document can be followed by another redirect</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
function generateResponseURL(origin, parameters)
{
    return origin + "/cache/disk-cache/resources/generate-response.cgi?" + new URLSearchParams(parameters);
}

promise_test(async () => {
    const uniqueId = Math.random();
    const finalURL = generateResponseURL("http://localhost:8000", { "Cache-Control": "max-age=100", "Content-Type": "text/plain", body: "PASS", uniqueId });
    const crossOriginURL = generateResponseURL("http://localhost:8000", { Status: "302", Location: finalURL, "Cache-Control": "no-store", uniqueId });
    const url = generateResponseURL("http://127.0.0.1:8000", { Status: "301", Location: crossOriginURL, "Cache-Control": "max-age=100", uniqueId });

    for (let i = 0; i < 2; ++i) {
        internals.clearMemoryCache();
        const response = await fetch(url, { mode: "no-cors", credentials: "include" });
        assert_equals(response.type, "opaque");
        // Give the disk cache time to store the responses.
        await new Promise(resolve => setTimeout(resolve, 100));
    }

    internals.clearMemoryCache();
    const redirect = await fetch(url, { redirect: "manual" });
    assert_equals(internals.fetchResponseSource(redirect), "Disk cache", "the redirect was stored");
});
</script>''')
