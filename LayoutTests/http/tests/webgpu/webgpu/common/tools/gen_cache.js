/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as fs from 'fs';import * as path from 'path';import * as process from 'process';

import { dataCache, setIsBuildingDataCache } from '../framework/data_cache.js';

function usage(rc) {
  console.error(`Usage: tools/gen_cache [options] [OUT_DIR] [SUITE_DIRS...]

For each suite in SUITE_DIRS, pre-compute data that is expensive to generate
at runtime and store it under OUT_DIR. If the data file is found then the
DataCache will load this instead of building the expensive data at CTS runtime.

Options:
  --help          Print this message and exit.
  --list          Print the list of output files without writing them.
`);
  process.exit(rc);
}

let mode = 'emit';

const nonFlagsArgs = [];
for (const a of process.argv) {
  if (a.startsWith('-')) {
    if (a === '--list') {
      mode = 'list';
    } else if (a === '--help') {
      usage(0);
    } else {
      console.log('unrecognized flag: ', a);
      usage(1);
    }
  } else {
    nonFlagsArgs.push(a);
  }
}

if (nonFlagsArgs.length < 4) {
  usage(0);
}

const outRootDir = nonFlagsArgs[2];

dataCache.setStore({
  load: (path) => {
    return new Promise((resolve, reject) => {
      fs.readFile(`data/${path}`, 'utf8', (err, data) => {
        if (err !== null) {
          reject(err.message);
        } else {
          resolve(data);
        }
      });
    });
  }
});
setIsBuildingDataCache();

void (async () => {
  for (const suiteDir of nonFlagsArgs.slice(3)) {
    await build(suiteDir);
  }
})();

const specFileSuffix = __filename.endsWith('.ts') ? '.spec.ts' : '.spec.js';

async function crawlFilesRecursively(dir) {
  const subpathInfo = await Promise.all(
  (await fs.promises.readdir(dir)).map(async (d) => {
    const p = path.join(dir, d);
    const stats = await fs.promises.stat(p);
    return {
      path: p,
      isDirectory: stats.isDirectory(),
      isFile: stats.isFile()
    };
  }));


  const files = subpathInfo.
  filter((i) => i.isFile && i.path.endsWith(specFileSuffix)).
  map((i) => i.path);

  return files.concat(
  await subpathInfo.
  filter((i) => i.isDirectory).
  map((i) => crawlFilesRecursively(i.path)).
  reduce(async (a, b) => (await a).concat(await b), Promise.resolve([])));

}

async function build(suiteDir) {
  if (!fs.existsSync(suiteDir)) {
    console.error(`Could not find ${suiteDir}`);
    process.exit(1);
  }

  // Crawl files and convert paths to be POSIX-style, relative to suiteDir.
  const filesToEnumerate = (await crawlFilesRecursively(suiteDir)).sort();

  const cacheablePathToTS = new Map();

  for (const file of filesToEnumerate) {
    if (file.endsWith(specFileSuffix)) {
      const pathWithoutExtension = file.substring(0, file.length - specFileSuffix.length);
      const mod = await import(`../../../${pathWithoutExtension}.spec.js`);
      if (mod.d?.serialize !== undefined) {
        const cacheable = mod.d;

        {
          // Check for collisions
          const existing = cacheablePathToTS.get(cacheable.path);
          if (existing !== undefined) {
            console.error(
            `error: Cacheable '${cacheable.path}' is emitted by both:
    '${existing}'
and
    '${file}'`);

            process.exit(1);
          }
          cacheablePathToTS.set(cacheable.path, file);
        }

        const outPath = `${outRootDir}/data/${cacheable.path}`;

        switch (mode) {
          case 'emit':{
              const data = await cacheable.build();
              const serialized = cacheable.serialize(data);
              fs.mkdirSync(path.dirname(outPath), { recursive: true });
              fs.writeFileSync(outPath, serialized);
              break;
            }
          case 'list':{
              console.log(outPath);
              break;
            }}

      }
    }
  }
}
//# sourceMappingURL=gen_cache.js.map