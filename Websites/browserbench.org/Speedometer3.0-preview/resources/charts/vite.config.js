import { resolve } from "path";
import { defineConfig } from "vite";
export default defineConfig({
    base: "./", // Since this will be loaded from the project root
    build: {
        modulePreload: { polyfill: false },
        minify: false,
        rollupOptions: {
            input: {
                index: resolve(__dirname, "index.html"),
                developer: resolve(__dirname, "developer.html"),
                plot: resolve(__dirname, "observable-plot.html"),
                chartjs: resolve(__dirname, "chartjs.html"),
            },
        },
    },
});
