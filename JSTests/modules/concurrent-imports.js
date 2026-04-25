const moduleUrl = "./concurrent-imports/module.js";

const logs = [];

async function loadDynamicModule(importIndex) {
  try {
    logs.push(`Starting import ${importIndex}`);
    const module = await import(moduleUrl);
    logs.push(`Import ${importIndex} completed`);

    try {
      logs.push(`Access ${importIndex}: ` + Object.keys(module));
    } catch (err) {
      logs.push(`Access ${importIndex} failed: ` + err.message);
    }
  } catch (error) {
    logs.push(`Import ${importIndex} failed: ` + error.message);
  }
}

const imports = Array.from({ length: 5 }, (_, i) => {
  const importIndex = i + 1;
  return loadDynamicModule(importIndex);
});

await Promise.all(imports);

const expected = [
    "Starting import 1",
    "Starting import 2",
    "Starting import 3",
    "Starting import 4",
    "Starting import 5",
    "Import 1 completed",
    "Access 1: someArray,someFunction",
    "Import 2 completed",
    "Access 2: someArray,someFunction",
    "Import 3 completed",
    "Access 3: someArray,someFunction",
    "Import 4 completed",
    "Access 4: someArray,someFunction",
    "Import 5 completed",
    "Access 5: someArray,someFunction"
];

if (logs.join("\n") != expected.join("\n")) {
  throw "Unexpected output";
}
