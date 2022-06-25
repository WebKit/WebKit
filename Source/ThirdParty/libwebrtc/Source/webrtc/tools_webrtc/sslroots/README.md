# Generate rtc_base/ssl_roots.h

This directory contains a script to generate the content of
[rtc_base/ssl_roots.h][ssl-roots-header], to update the SSL roots shipped
by WebRTC follow this instructions:

1. Download roots.pem from [pki.goog][pki-goog].

2. Launch the script:

```
$ python tools_webrtc/sslroots/generate_sslroots.py roots.pem
```

3. Step 2 should have generated an ssl_roots.h file right next to roots.pem.

4. Open rtc_base/ssl_roots.h, manually remove the old certificates and paste
   the ones from the ssl_roots.h file.

5. Delete the generated ssl_roots.h and roots.pem before creating the CL.

[ssl-roots-header]: https://cs.chromium.org/chromium/src/third_party/webrtc/rtc_base/ssl_roots.h
[pki-goog]: https://www.google.com/url?q=https://pki.google.com/roots.pem
