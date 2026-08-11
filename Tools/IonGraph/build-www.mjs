import * as esbuild from "esbuild";
import { readdirSync, copyFileSync, statSync } from "fs";
import { join, relative, resolve } from "path";
import { mkdirSync, readFileSync, rmSync, watch, writeFileSync } from "fs";

const outDir = "dist-www";

const staticFileDirs = ["www", "src"];
const staticFilePattern = /\.(html|css|json)$/;

const reWholeDirective = /\{\{\s*(("[^"]*"|[a-zA-Z0-9_-]+)\s*)+\}\}/g;
const reDirectiveToken = /"([^"]*)"|([a-zA-Z0-9_-]+)/g;

function findFiles(dir, matches, result) {
  for (const file of readdirSync(dir)) {
    const filepath = join(dir, file);
    const stat = statSync(filepath);
    if (stat.isDirectory()) {
      findFiles(filepath, result);
    } else if (matches.test(file)) {
      result.push(filepath);
    }
  }
}

function copyStaticFiles() {
  for (const fromDir of staticFileDirs) {
    const files = [];
    findFiles(fromDir, staticFilePattern, files);
    for (const file of files) {
      const dest = join(outDir, relative(fromDir, file));
      copyFileSync(file, dest);
    }
  }
}

console.log(`Clearing ${outDir}...`);
rmSync(outDir, { recursive: true, force: true });
mkdirSync(outDir, { recursive: true });
console.log("Copying static files...");
copyStaticFiles();

const baseConfig = {
  entryPoints: ["www/main.ts"],
  outdir: outDir,
  bundle: true,
  target: ["es2020"],
};
const moduleConfig = {
  ...baseConfig,
  format: "esm",
  sourcemap: true,
};
const standaloneConfig = {
  ...baseConfig,
  format: "iife",
  globalName: "iongraph",
  minify: true,
  outExtension: { ".js": ".standalone.js" },
};

console.log("Running esbuild...");
await esbuild.build(standaloneConfig);
const ctx = await esbuild.context(moduleConfig);

console.log("Building standalone HTML...");
const standaloneContent = buildStandalone();

// Only build the bundled index.html for production builds, not for --serve
if (!process.argv.includes("--serve")) {
  console.log("Building index HTML...");
  buildIndex(standaloneContent);
}

if (process.argv.includes("--test-standalone")) {
  const template = readFileSync(join(outDir, "standalone.html"), "utf-8");
  const json = readFileSync("/tmp/ion.json");
  const formatted = template.replace(/\{\{\s*IONJSON\s*\}\}/, json);
  writeFileSync(join(outDir, "standalone-test.html"), formatted);
}

if (process.argv.includes("--serve")) {
  await ctx.watch();
  const { hosts, port } = await ctx.serve({
    servedir: "./dist-www/",
  });
  console.log(`Now serving on http://localhost:${port}`);

  for (const dir of ["www", "src"]) {
    watch(dir, { persistent: false, recursive: true }, (eventType, filename) => {
      if (staticFilePattern.test(filename)) {
        console.log(`Copying static files due to change in: ${filename}`);
        copyStaticFiles();
      }
    });
  }
} else {
  await ctx.rebuild();
  await ctx.dispose();
  console.log("Built successfully.");
}

function buildStandalone() {
  const template = readFileSync("www/standalone.html", "utf8");
  const processed = template.replaceAll(reWholeDirective, directive => {
    const parsed = parseDirective(directive);
    switch (parsed[0]) {
      case "include-dist": {
        const resolved = resolve("dist-www", parsed[1]);
        return readFileSync(resolved, "utf8");
      }
      default:
        return directive;
    }
  });
  writeFileSync(join(outDir, "standalone.html"), processed);
  return processed;
}

function buildIndex(standaloneContent) {
  const template = readFileSync("www/index.built.html", "utf8");
  const processed = template.replaceAll(reWholeDirective, directive => {
    const parsed = parseDirective(directive);
    switch (parsed[0]) {
      case "include-dist": {
        const resolved = resolve("dist-www", parsed[1]);
        return readFileSync(resolved, "utf8");
      }
      case "standalone-template-json": {
        // Base64 encode to avoid any HTML parsing issues
        return `"${Buffer.from(standaloneContent).toString("base64")}"`;
      }
      default:
        return directive;
    }
  });
  writeFileSync(join(outDir, "index.html"), processed);
}

function parseDirective(directive) {
  const res = [];
  for (const match of directive.matchAll(reDirectiveToken)) {
    if (match[1]) {
      res.push(match[1]);
    } else {
      res.push(match[2]);
    }
  }
  return res;
}
