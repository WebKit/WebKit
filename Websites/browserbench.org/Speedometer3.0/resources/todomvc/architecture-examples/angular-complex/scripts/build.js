/**
 * Builds the TodoMVC Angular Complex DOM.
 */
const path = require("path");
const { buildComplex } = require("big-dom-generator/utils/buildComplex");

const options = {
    callerDirectory: path.resolve(__dirname),
    sourceDirectory: path.join("..", "node_modules", "todomvc-angular", "dist"),
    title: "TodoMVC: Angular Complex DOM",
    filesToMove: ["node_modules/big-dom-generator/dist/big-dom-with-stacking-context-scrollable.css", "node_modules/big-dom-generator/dist/logo.png"],
    cssFilePath: path.resolve(__dirname, "..", "node_modules", "big-dom-generator", "utils", "app.css"),
    cssFileNamePattern: /^styles.*\.css$/,
    standaloneDirectory: path.resolve(__dirname, "..", "..", "angular"),
    complexDirectory: path.resolve(__dirname, ".."),
    cssFilesToAddLinksFor: ["big-dom-with-stacking-context-scrollable.css"],
};

buildComplex(options);
