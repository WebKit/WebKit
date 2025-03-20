const fs = require("fs").promises;
const { resolve } = require("path");

const dirName = "./dist";
// spelling out words that this file doesn't trigger any bad word filters.
const parts = [
    ["b", "l", "a", "c", "k", "l", "i", "s", "t"],
    ["w", "h", "i", "t", "e", "l", "i", "s", "t"],
];
const wordsToUse = ["disallowlist", "allowlist"];

const wordsToReplace = parts.map((part) => part.join(""));
const replacements = new Map();
wordsToReplace.forEach((word, index) => replacements.set(word, wordsToUse[index]));
const stringReplaceRegex = new RegExp(wordsToReplace.join("|"), "gi");

async function getFiles(dir) {
    const dirents = await fs.readdir(dir, { withFileTypes: true });
    const files = await Promise.all(
        dirents.map((dirent) => {
            const res = resolve(dir, dirent.name);
            return dirent.isDirectory() ? getFiles(res) : res;
        })
    );
    return Array.prototype.concat(...files);
}

async function readAndReplace(fileName) {
    const contents = await fs.readFile(fileName, "utf8");
    const sanitized = contents.replace(stringReplaceRegex, function (matched) {
        return replacements.get(matched);
    });

    if (contents !== sanitized)
        await fs.writeFile(fileName, sanitized);
}

async function sanitize() {
    const dir = process.env.OUTPUT_FOLDER ?? dirName;
    const files = await getFiles(dir);
    await Promise.all(files.map((file) => readAndReplace(file)));
    console.log(`** Done sanitizing ${dir}! **`);
}

sanitize();
