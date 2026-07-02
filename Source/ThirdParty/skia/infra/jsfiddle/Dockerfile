FROM gcr.io/skia-public/jsfiddle-base@sha256:268fe17ee4d6ffa2d3ed0d054af07926bd765f8b48e7d5679fc18110be7495dc

COPY canvaskit.js /usr/local/share/jsfiddle/dist/canvaskit.js
COPY canvaskit.wasm /usr/local/share/jsfiddle/dist/canvaskit.wasm
COPY version.js /usr/local/share/jsfiddle/dist/version.js
COPY index.d.ts /usr/local/share/jsfiddle/dist/index.d.ts

CMD ["/usr/local/bin/jsfiddle", "--help"]