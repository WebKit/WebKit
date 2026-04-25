FROM gcr.io/skia-public/skottie-base@sha256:36b2e3d49150c33d2b62c1b1ea5f7fafc24a6d81dd32b71bd709e81199596500

COPY canvaskit.js /usr/local/share/skottie/canvaskit.js
COPY canvaskit.wasm /usr/local/share/skottie/canvaskit.wasm
COPY version.js /usr/local/share/skottie/version.js

CMD ["/usr/local/bin/skottie", "--help"]