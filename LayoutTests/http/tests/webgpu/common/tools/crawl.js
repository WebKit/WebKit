/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/ // Node can look at the filesystem, but JS in the browser can't.
// This crawls the file tree under src/suites/${suite} to generate a (non-hierarchical) static
// listing file that can then be used in the browser to load the modules containing the tests.
import * as fs from 'fs';import * as path from 'path';


import { validQueryPart } from '../internal/query/validQueryPart.js';

import { assert, unreachable } from '../util/util.js';

const specFileSuffix = __filename.endsWith('.ts') ? '.spec.ts' : '.spec.js';

async function crawlFilesRecursively(dir) {
  const subpathInfo = await Promise.all(
  (await fs.promises.readdir(dir)).map(async (d) => {
    const p = path.join(dir, d);
    const stats = await fs.promises.stat(p);
    return {
      path: p,
      isDirectory: stats.isDirectory(),
      isFile: stats.isFile() };

  }));


  const files = subpathInfo.
  filter(
  (i) =>
  i.isFile && (
  i.path.endsWith(specFileSuffix) ||
  i.path.endsWith(`${path.sep}README.txt`) ||
  i.path === 'README.txt')).

  map((i) => i.path);

  return files.concat(
  await subpathInfo.
  filter((i) => i.isDirectory).
  map((i) => crawlFilesRecursively(i.path)).
  reduce(async (a, b) => (await a).concat(await b), Promise.resolve([])));

}

export async function crawl(
suiteDir,
validate = true)
{
  if (!fs.existsSync(suiteDir)) {
    console.error(`Could not find ${suiteDir}`);
    process.exit(1);
  }

  // Crawl files and convert paths to be POSIX-style, relative to suiteDir.
  const filesToEnumerate = (await crawlFilesRecursively(suiteDir)).
  map((f) => path.relative(suiteDir, f).replace(/\\/g, '/')).
  sort();

  const entries = [];
  for (const file of filesToEnumerate) {
    // |file| is the suite-relative file path.
    if (file.endsWith(specFileSuffix)) {
      const filepathWithoutExtension = file.substring(0, file.length - specFileSuffix.length);

      const suite = path.basename(suiteDir);

      if (validate) {
        const filename = `../../${suite}/${filepathWithoutExtension}.spec.js`;

        assert(!process.env.STANDALONE_DEV_SERVER);
        const mod = await import(filename);
        assert(mod.description !== undefined, 'Test spec file missing description: ' + filename);
        assert(mod.g !== undefined, 'Test spec file missing TestGroup definition: ' + filename);

        mod.g.validate();
      }

      const pathSegments = filepathWithoutExtension.split('/');
      for (const p of pathSegments) {
        assert(validQueryPart.test(p), `Invalid directory name ${p}; must match ${validQueryPart}`);
      }
      entries.push({ file: pathSegments });
    } else if (path.basename(file) === 'README.txt') {
      const dirname = path.dirname(file);
      const readme = fs.readFileSync(path.join(suiteDir, file), 'utf8').trim();

      const pathSegments = dirname !== '.' ? dirname.split('/') : [];
      entries.push({ file: pathSegments, readme });
    } else {
      unreachable(`Matched an unrecognized filename ${file}`);
    }
  }

  return entries;
}

export function makeListing(filename) {
  // Don't validate. This path is only used for the dev server and running tests with Node.
  // Validation is done for listing generation and presubmit.
  return crawl(path.dirname(filename), false);
}
//# sourceMappingURL=crawl.js.map