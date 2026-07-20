FROM gcr.io/skia-public/shaders-base@sha256:33357ebae78cc5cb17a3138d1e5eb95365f2fbe8b19040dbf7ec3df90d3f73a7

COPY canvaskit.js /usr/local/share/shaders/dist/canvaskit.js
COPY canvaskit.wasm /usr/local/share/shaders/dist/canvaskit.wasm
COPY version.js /usr/local/share/shaders/dist/version.js

CMD ["/usr/local/bin/shaders", "--help"]