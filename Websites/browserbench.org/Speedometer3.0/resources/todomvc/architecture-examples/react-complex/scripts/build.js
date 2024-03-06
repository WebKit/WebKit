/**
 * Build the TodoMVC: React Complex DOM example.
 */
const path = require("path");
const { buildComplex } = require("big-dom-generator/utils/buildComplex");

const options = {
    callerDirectory: path.resolve(__dirname),
    sourceDirectory: path.join("..", "node_modules", "todomvc-react", "dist"),
    title: "TodoMVC: React Complex DOM",
    filesToMove: ["node_modules/big-dom-generator/dist/big-dom.css", "node_modules/big-dom-generator/dist/logo.png", "node_modules/big-dom-generator/utils/app.css"],
    standaloneDirectory: path.resolve(__dirname, "..", "..", "react"),
    complexDirectory: path.resolve(__dirname, ".."),
};

buildComplex(options);
