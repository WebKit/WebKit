FROM gcr.io/skia-public/skottie-base@sha256:4b069fbc5e1a5ffac80bae3a48ff46d93b2d9ef9fb16173eea1d142024db3251

COPY canvaskit.js /usr/local/share/skottie/canvaskit.js
COPY canvaskit.wasm /usr/local/share/skottie/canvaskit.wasm
COPY version.js /usr/local/share/skottie/version.js

CMD ["/usr/local/bin/skottie", "--help"]