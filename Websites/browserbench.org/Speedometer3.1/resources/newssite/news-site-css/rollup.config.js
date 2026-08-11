import terser from "@rollup/plugin-terser";
import resolve from "@rollup/plugin-node-resolve";
import commonjs from "@rollup/plugin-commonjs";
import babel from "@rollup/plugin-babel";
import css from "rollup-plugin-import-css";
import copy from "rollup-plugin-copy-merge";
import cleaner from "rollup-plugin-cleaner";

// `npm run build` -> `production` is true
// `npm run dev` -> `production` is false
const production = !process.env.ROLLUP_WATCH;

export default {
    input: "src/js/index.js",
    output: [
        {
            file: "dist/index.js",
            format: "es",
        },
    ],
    plugins: [
        cleaner({
            targets: ["./dist/"],
        }),
        css({
            minify: true,
            output: "dist/index.min.css",
        }),
        babel({
            babelrc: false,
        }),
        resolve({
            jsnext: true,
            main: true,
            browser: true,
        }),
        commonjs(),
        copy({
            targets: [
                {
                    src: ["src/css/*", "!src/css/partials.css", "!src/css/global.css"],
                    dest: "dist/",
                    rename: (name, extension) => `${name}.module.${extension}`,
                },
                /* {
                    src: ["src/css/*", "!src/css/partials.css"],
                    file: "dist/index.css"
                }, */
                {
                    src: [
                        "src/css/variables.css",
                        "src/css/global.css",
                        "src/css/icons.css",
                        "src/css/icons-group.css",
                        "src/css/button.css",
                        "src/css/a11y.css",
                        "src/css/input.css",
                        "src/css/form.css",
                        "src/css/layout.css",
                        "src/css/advertisement.css",
                        "src/css/header.css",
                        "src/css/nav.css",
                        "src/css/navbar.css",
                        "src/css/footer.css",
                        "src/css/dialog.css",
                        "src/css/dropdown.css",
                        "src/css/article.css",
                        "src/css/text.css",
                        "src/css/toggle.css",
                        "src/css/toast.css",
                        "src/css/sitemap.css",
                        "src/css/message.css",
                        "src/css/sidebar.css",
                        "src/css/modal.css",
                    ],
                    file: "dist/index.css",
                },
                {
                    src: ["src/css/*"],
                    dest: "dist/",
                },
            ],
        }),
        production && terser(),
    ],
};
