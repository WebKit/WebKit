/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as fs from 'fs';import * as path from 'path';import * as process from 'process';

import { crawl } from './crawl.js';

function usage(rc) {
  console.error(`Usage: tools/gen_listings [options] [OUT_DIR] [SUITE_DIRS...]

For each suite in SUITE_DIRS, generate listings and write each listing.js
into OUT_DIR/{suite}/listing.js. Example:
  tools/gen_listings out/ src/unittests/ src/webgpu/

Options:
  --help          Print this message and exit.
  --no-validate   Whether to validate test modules while crawling.
`);
  process.exit(rc);
}

const argv = process.argv;
if (argv.indexOf('--help') !== -1) {
  usage(0);
}

let validate = true;
{
  const i = argv.indexOf('--no-validate');
  if (i !== -1) {
    validate = false;
    argv.splice(i, 1);
  }
}

if (argv.length < 4) {
  usage(0);
}

const myself = 'src/common/tools/gen_listings.ts';

const outDir = argv[2];

void (async () => {
  for (const suiteDir of argv.slice(3)) {
    const listing = await crawl(suiteDir, validate);

    const suite = path.basename(suiteDir);
    const outFile = path.normalize(path.join(outDir, `${suite}/listing.js`));
    fs.mkdirSync(path.join(outDir, suite), { recursive: true });
    fs.writeFileSync(
    outFile,
    `\
// AUTO-GENERATED - DO NOT EDIT. See ${myself}.

export const listing = ${JSON.stringify(listing, undefined, 2)};
`);

    try {
      fs.unlinkSync(outFile + '.map');
    } catch (ex) {

      // ignore if file didn't exist
    }}
})();
//# sourceMappingURL=gen_listings.js.map