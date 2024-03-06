/**
 * Builds the TodoMVC Vue Complex DOM.
 */
const path = require("path");
const { buildComplex } = require("big-dom-generator/utils/buildComplex");

const options = {
    callerDirectory: path.resolve(__dirname),
    sourceDirectory: path.join("..", "node_modules", "todomvc-vue", "dist"),
    title: "TodoMVC: Vue Complex DOM",
    filesToMove: ["node_modules/big-dom-generator/dist/big-dom.css", "node_modules/big-dom-generator/dist/logo.png"],
    cssFilePath: path.resolve(__dirname, "..", "node_modules", "big-dom-generator", "utils", "app.css"),
    cssFolder: "css",
    cssFileNamePattern: /^app.*\.css$/,
    standaloneDirectory: path.resolve(__dirname, "..", "..", "vue"),
    complexDirectory: path.resolve(__dirname, ".."),
};

buildComplex(options);
