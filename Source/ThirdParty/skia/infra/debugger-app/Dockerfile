FROM gcr.io/skia-public/debugger-app-base@sha256:13903d912e2dc54c9b3d86db2f8a929957f58cfd2f2343960c0e9df77dc6442d

COPY canvaskit.js /usr/local/share/debugger-app/canvaskit.js
COPY canvaskit.wasm /usr/local/share/debugger-app/canvaskit.wasm
COPY version.js /usr/local/share/debugger-app/version.js
COPY index.d.ts /usr/local/share/debugger-app/index.d.ts

CMD ["/usr/local/bin/debugger", "--help"]