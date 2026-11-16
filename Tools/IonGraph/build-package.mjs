import { spawn } from "child_process";
import { copyFileSync, rmSync } from "fs";
import { join } from "path";

rmSync("dist", { recursive: true, force: true });
await runTsc();
copyCss();
console.log("Done.");

function runTsc() {
  console.log("Running tsc...");
  return new Promise((resolve, reject) => {
    const tsc = spawn("./node_modules/.bin/tsc", [], { stdio: "inherit" });
    tsc.on("close", code => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`tsc exited with code ${code}`));
      }
    });
    tsc.on("error", err => {
      reject(err);
    });
  });
}

function copyCss() {
  console.log("Copying css to build folder...");
  const src = join("src", "style.css");
  const destDir = "dist";
  const dest = join(destDir, "style.css");
  copyFileSync(src, dest);
}
