import { resolve } from "path";
import { defineConfig } from "vite";
export default defineConfig({
    base: "./", // Since this will be loaded from the project root
    build: {
        modulePreload: { polyfill: false },
        minify: false,
        rollupOptions: {
            input: {
                codemirror: resolve(__dirname, "codemirror.html"),
                main: resolve(__dirname, "index.html"),
                tiptap: resolve(__dirname, "tiptap.html"),
            },
        },
    },
});
