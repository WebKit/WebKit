/**
 * Builds the TodoMVC JavaScript Web Components Complex DOM.
 */
const path = require("path");
const { buildComplex } = require("big-dom-generator/utils/buildComplex");

const options = {
    callerDirectory: path.resolve(__dirname),
    sourceDirectory: path.join("..", "node_modules", "todomvc-javascript-web-components", "dist"),
    title: "TodoMVC: JavaScript Web Components Complex DOM",
    filesToMove: [
        "node_modules/big-dom-generator/dist/big-dom.css",
        "node_modules/big-dom-generator/dist/logo.png",
        "node_modules/big-dom-generator/utils/web-components-css/app.css",
        "node_modules/big-dom-generator/utils/web-components-css/default-variables.css",
        "node_modules/big-dom-generator/utils/web-components-css/vanilla/todo-item-extra-css.js",
        "node_modules/big-dom-generator/utils/web-components-css/todo-list-extra-css.js",
    ],
    extraCssToLink: ["app.css", "default-variables.css"],
    scriptsToLink: ["todo-item-extra-css.js", "todo-list-extra-css.js"],
    standaloneDirectory: path.resolve(__dirname, "..", "..", "javascript-web-components"),
    complexDirectory: path.resolve(__dirname, ".."),
};

buildComplex(options);
