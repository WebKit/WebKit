#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cross-Origin-Opener-Policy: unsafe-none\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<meta charset="utf-8">
<script>
const channelName = new URL(location).searchParams.get("channel");
const channel = new BroadcastChannel(channelName);

channel.onmessage = event => {
    if (event.data === "close") {
        channel.close();
        window.close();
    }
};

channel.postMessage({
    hasOpener: window.opener !== null,
});
</script>''')
