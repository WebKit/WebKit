/**
 * Builds the TodoMVC JavaScript Es6 Webpack Complex DOM.
 */
const path = require("path");
const { buildComplex } = require("big-dom-generator/utils/buildComplex");

const options = {
    callerDirectory: path.resolve(__dirname),
    sourceDirectory: path.join("..", "node_modules", "todomvc-javascript-es6-webpack", "dist"),
    title: "TodoMVC: JavaScript Es6 Webpack Complex DOM",
    filesToMove: ["node_modules/big-dom-generator/dist/big-dom.css", "node_modules/big-dom-generator/dist/logo.png", "node_modules/big-dom-generator/utils/app.css"],
    standaloneDirectory: path.resolve(__dirname, "..", "..", "javascript-es6-webpack"),
    complexDirectory: path.resolve(__dirname, ".."),
};

buildComplex(options);
