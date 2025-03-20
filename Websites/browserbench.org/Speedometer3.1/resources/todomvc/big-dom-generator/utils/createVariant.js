/**
 * Create a variant of big-dom.css by adding a property
 * that creates a CSS stacking context for the overflow scroller (<div class="tree-area">)
 * to trigger different code paths related to scrolling in browsers.
 */
try {
    const fs = require("fs");
    const postcss = require("postcss");

    const INPUT_FILE_PATH = "./dist/big-dom.css";
    const OUTPUT_FILE_PATH = "./dist/big-dom-with-stacking-context-scrollable.css";

    const css = fs.readFileSync(INPUT_FILE_PATH, "utf-8");
    const root = postcss.parse(css, { from: INPUT_FILE_PATH });
    root.walkRules(".tree-area", (rule) => {
        rule.append({ prop: "isolation", value: "isolate" });
    });
    fs.writeFileSync(OUTPUT_FILE_PATH, root.toString());
} catch (error) {
    console.error("An error occurred while generating the big dom CSS variant:", error);
}
