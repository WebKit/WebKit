FROM gcr.io/skia-public/shaders-base@sha256:1537a8ed61c99568d75ba29cef9a9d6e35420acfd9120d28ff04cc2941b58a0c

COPY canvaskit.js /usr/local/share/shaders/dist/canvaskit.js
COPY canvaskit.wasm /usr/local/share/shaders/dist/canvaskit.wasm
COPY version.js /usr/local/share/shaders/dist/version.js

CMD ["/usr/local/bin/shaders", "--help"]