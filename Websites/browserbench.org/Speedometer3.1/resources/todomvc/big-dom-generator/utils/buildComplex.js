const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");

/**
 * Builds the complex version of TodoMVC.
 * @param {Object} options - The options for building the complex version.
 * @param {string} options.callerDirectory - The directory of the caller.
 * @param {string} options.sourceDirectory - The directory of the source files.
 * @param {string} options.title - The title of the generated HTML file.
 * @param {string[]} options.filesToMove - An array of file paths to move to the dist directory.
 * @param {string} options.cssFilePath - The path to the CSS file.
 * @param {string} [options.cssFolder=""] - The folder where the CSS file is located.
 * @param {RegExp} [options.cssFileNamePattern - The css file name pattern is used to find the css file in the source dist directory.
 * @param {string[]} options.extraCssToLink=[] - An array of extra CSS files to link.
 * @param {string[]} options.scriptsToLink=[] - An array of scripts to link.
 * @param {string} options.targetDirectory="./dist" - The target directory.
 * @param {string} options.complexDomHtmlFile="index.html" - The name of the complex HTML file.
 * @param {string} options.todoHtmlFile="index.html" - The name of the todo HTML file.
 * @param {string[]} options.cssFilesToAddLinksFor=["big-dom.css"] - An array of CSS files to add links for.
 * @param {string} options.standaloneDirectory - The directory of the TodoMVC standalone version.
 * @param {string} options.complexDirectory - The directory of the TodoMVC complex version.
 */
function buildComplex(options) {
    const {
        callerDirectory,
        sourceDirectory,
        title,
        filesToMove,
        cssFilePath,
        cssFolder = "", // sometimes the css file we are looking for may be nested in another folder.
        cssFileNamePattern, // This is mandatory if cssFilePath is provided.
        extraCssToLink = [],
        scriptsToLink = [],
        targetDirectory = "./dist",
        complexDomHtmlFile = "index.html",
        todoHtmlFile = "index.html",
        cssFilesToAddLinksFor = ["big-dom.css"],
    } = options;

    prepareComplex(options);

    // npm ci in big-dom-generator needs to run before we import JSDOM
    let JSDOM;
    try {
        JSDOM = require("jsdom").JSDOM;
    } catch (e) {
        console.error("Error: jsdom is not installed.");
        process.exit(1);
    }

    // Remove dist directory if it exists
    fs.rmSync(path.resolve(targetDirectory), { recursive: true, force: true });

    // Re-create the directory
    fs.mkdirSync(path.resolve(targetDirectory));

    // Copy dist folder from javascript-es6-webpack
    fs.cpSync(path.join(callerDirectory, sourceDirectory), path.resolve(targetDirectory), { recursive: true });

    // Copy files to move
    for (let i = 0; i < filesToMove.length; i++) {
        // Rename app.css to big-dom.css so it's unique
        const sourcePath = path.resolve(callerDirectory, "..", filesToMove[i]);
        const fileName = path.basename(filesToMove[i]);
        const targetPath = path.join(targetDirectory, fileName);
        fs.copyFileSync(sourcePath, targetPath);
    }

    if (cssFilePath) {
        // Get the name of the CSS file that's in the dist, we do this because the name of the CSS file may change
        const cssFolderDirectory = path.join(callerDirectory, sourceDirectory, cssFolder);
        const cssFile = fs.readdirSync(cssFolderDirectory, { withFileTypes: true }).find((dirent) => dirent.isFile() && cssFileNamePattern.test(dirent.name))?.name;
        // Overwrite the CSS file in the dist directory with the one from the big-dom-generator module
        // but keep the existing name so we don't need to add a new link
        fs.copyFileSync(cssFilePath, path.resolve(targetDirectory, cssFolder, cssFile));
    }

    // Read todo.html file
    let html = fs.readFileSync(path.resolve(callerDirectory, path.join("..", "dist", todoHtmlFile)), "utf8");

    const dom = new JSDOM(html);
    const doc = dom.window.document;
    const head = doc.querySelector("head");

    doc.documentElement.setAttribute("class", "spectrum spectrum--medium spectrum--light");

    const body = doc.querySelector("body");
    const htmlToInjectInTodoHolder = body.innerHTML;
    body.innerHTML = getHtmlBodySync("node_modules/big-dom-generator/dist/index.html");

    const titleElement = head.querySelector("title");
    titleElement.innerHTML = title;

    const todoHolder = doc.createElement("div");
    todoHolder.className = "todoholder";
    todoHolder.innerHTML = htmlToInjectInTodoHolder;

    const todoArea = doc.querySelector(".todo-area");
    todoArea.appendChild(todoHolder);

    const cssFilesToAddLinksForFinal = [...cssFilesToAddLinksFor, ...extraCssToLink];
    for (const cssFile of cssFilesToAddLinksForFinal) {
        const cssLink = doc.createElement("link");
        cssLink.rel = "stylesheet";
        cssLink.href = cssFile;
        head.appendChild(cssLink);
    }

    for (const script of scriptsToLink) {
        const scriptLink = doc.createElement("script");
        scriptLink.src = script;
        head.appendChild(scriptLink);
    }

    const destinationFilePath = path.join(targetDirectory, complexDomHtmlFile);
    fs.writeFileSync(destinationFilePath, dom.serialize());

    console.log(`The complex code for ${sourceDirectory} has been written to ${destinationFilePath}.`);
}

/**
 * Performs the prerequisite steps for building the complex version.
 * @param {Object} options - The options for building the complex version.
 * @param {string} [options.standaloneDirectory] - The directory of the TodoMVC standalone version.
 * @param {string} [options.complexDirectory] - The directory of the TodoMVC complex version.
 */
function prepareComplex(options) {
    const { standaloneDirectory, complexDirectory } = options;

    // Run npm i in big-dom-generator
    console.log("Running npm ci in big-dom-generator...");
    execSync("npm ci", { cwd: path.join(__dirname, ".."), stdio: "inherit" });

    // Run npm i in the standalone directory
    console.log(`Running npm ci in the standalone directory: ${standaloneDirectory}`);
    execSync("npm ci", { cwd: standaloneDirectory, stdio: "inherit" });

    // Run npm run build in the standalone directory
    console.log(`Running npm run build in the standalone directory: ${standaloneDirectory}`);
    execSync("npm run build", { cwd: standaloneDirectory, stdio: "inherit" });

    console.log(`Running npm ci in the complex directory: ${complexDirectory}`);
    execSync("npm ci", { cwd: complexDirectory, stdio: "inherit" });
}

/**
 * Gets the HTML body from a file.
 * @param {string} filePath - The path of the file.
 * @returns {string} The HTML body.
 */
function getHtmlBodySync(filePath) {
    let htmlContent = fs.readFileSync(filePath, "utf8");
    const bodyStartIndex = htmlContent.indexOf("<body>");
    const bodyEndIndex = htmlContent.lastIndexOf("</body>");

    return htmlContent.substring(bodyStartIndex + 6, bodyEndIndex);
}

module.exports = { buildComplex };
