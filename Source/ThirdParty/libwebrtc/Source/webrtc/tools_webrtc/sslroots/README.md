# Generate rtc_base/ssl_roots.h

This directory contains a script to generate the content of
[rtc_base/ssl_roots.h][ssl-roots-header], to update the SSL roots shipped
by WebRTC follow this instructions:

1. Download roots.pem from [pki.goog][pki-goog] or [curl.se][mozilla-cacert]

2. Launch the script:

```
$ vpython3 tools_webrtc/sslroots/generate_sslroots.py <the pem file>
```

3. Step 2 should have generated an ssl_roots.h file right next to the pem file.

4. Overwrite rtc_base/ssl_roots.h with the newly generated one.

[ssl-roots-header]: https://cs.chromium.org/chromium/src/third_party/webrtc/rtc_base/ssl_roots.h
[pki-goog]: https://www.google.com/url?q=https://pki.google.com/roots.pem
[mozila-cacert]: https://curl.se/ca/cacert.pem
