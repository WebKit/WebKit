const fs = require("fs").promises;
const getDirName = require("path").dirname;

const rootDirectory = "./";
const sourceDirectory = "./src";
const targetDirectory = "./dist";

const htmlFile = "index.html";

const filesToMove = {
    index: [
        { src: "node_modules/todomvc-css/dist/global.css", dest: "styles/global.css" },
        { src: "node_modules/todomvc-css/dist/header.css", dest: "styles/header.css" },
        { src: "node_modules/todomvc-css/dist/footer.css", dest: "styles/footer.css" },
    ],
    app: [
        { src: "node_modules/todomvc-css/dist/global.constructable.js", dest: "styles/global.constructable.js" },
        { src: "node_modules/todomvc-css/dist/app.constructable.js", dest: "styles/app.constructable.js" },
        { src: "node_modules/todomvc-css/dist/topbar.constructable.js", dest: "styles/topbar.constructable.js" },
        { src: "node_modules/todomvc-css/dist/main.constructable.js", dest: "styles/main.constructable.js" },
        { src: "node_modules/todomvc-css/dist/bottombar.constructable.js", dest: "styles/bottombar.constructable.js" },
        { src: "node_modules/todomvc-css/dist/todo-list.constructable.js", dest: "styles/todo-list.constructable.js" },
        { src: "node_modules/todomvc-css/dist/todo-item.constructable.js", dest: "styles/todo-item.constructable.js" },
    ],
};

const importsToRename = {
    src: "../../../node_modules/todomvc-css/dist/",
    dest: "../../styles/",
    files: [
        "components/todo-app/todo-app.component.js",
        "components/todo-bottombar/todo-bottombar.component.js",
        "components/todo-item/todo-item.component.js",
        "components/todo-list/todo-list.component.js",
        "components/todo-topbar/todo-topbar.component.js",
    ],
};

const copy = async (src, dest) => {
    await fs.mkdir(getDirName(dest), { recursive: true });
    await fs.copyFile(src, dest);
};

const copyFilesToMove = async (files) => {
    for (let i = 0; i < files.length; i++)
        await copy(files[i].src, `${targetDirectory}/${files[i].dest}`);
};

const updateImports = async ({ file, src, dest }) => {
    let contents = await fs.readFile(`${targetDirectory}/${file}`, "utf8");
    contents = contents.replaceAll(src, dest);
    await fs.writeFile(`${targetDirectory}/${file}`, contents);
};

const build = async () => {
    // remove dist directory if it exists
    await fs.rm(targetDirectory, { recursive: true, force: true });

    // re-create the directory.
    await fs.mkdir(targetDirectory);

    // copy src folder
    await fs.cp(sourceDirectory, targetDirectory, { recursive: true }, (err) => {
        if (err)
            console.error(err);
    });

    // copy files to Move
    for (const key in filesToMove)
        copyFilesToMove(filesToMove[key]);

    // read html file
    let contents = await fs.readFile(`${rootDirectory}/${htmlFile}`, "utf8");

    // remove base paths from files to move
    const filesToMoveForIndex = filesToMove.index;
    for (let i = 0; i < filesToMoveForIndex.length; i++)
        contents = contents.replace(filesToMoveForIndex[i].src, filesToMoveForIndex[i].dest);

    // remove basePath from source directory
    const basePath = `${sourceDirectory.split("/")[1]}/`;
    const re = new RegExp(basePath, "g");
    contents = contents.replace(re, "");

    // write html files
    await fs.writeFile(`${targetDirectory}/${htmlFile}`, contents);

    // rename imports in modules
    importsToRename.files.forEach((file) => updateImports({ file, src: importsToRename.src, dest: importsToRename.dest }));

    console.log("done!!");
};

build();
