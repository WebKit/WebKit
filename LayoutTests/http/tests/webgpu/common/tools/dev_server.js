/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as fs from 'fs';import * as os from 'os';import * as path from 'path';

import * as babel from '@babel/core';
import * as chokidar from 'chokidar';
import * as express from 'express';
import * as morgan from 'morgan';
import * as portfinder from 'portfinder';
import * as serveIndex from 'serve-index';

import { makeListing } from './crawl.js';

// Make sure that makeListing doesn't cache imported spec files. See crawl().
process.env.STANDALONE_DEV_SERVER = '1';

const srcDir = path.resolve(__dirname, '../../');

// Import the project's babel.config.js. We'll use the same config for the runtime compiler.
const babelConfig = {
  ...require(path.resolve(srcDir, '../babel.config.js'))({
    cache: () => {

      /* not used */}
  }),
  sourceMaps: 'inline'
};

// Caches for the generated listing file and compiled TS sources to speed up reloads.
// Keyed by suite name
const listingCache = new Map();
// Keyed by the path to the .ts file, without src/
const compileCache = new Map();

console.log('Watching changes in', srcDir);
const watcher = chokidar.watch(srcDir, {
  persistent: true
});

/**
 * Handler to dirty the compile cache for changed .ts files.
 */
function dirtyCompileCache(absPath, stats) {
  const relPath = path.relative(srcDir, absPath);
  if ((stats === undefined || stats.isFile()) && relPath.endsWith('.ts')) {
    const tsUrl = relPath;
    if (compileCache.has(tsUrl)) {
      console.debug('Dirtying compile cache', tsUrl);
    }
    compileCache.delete(tsUrl);
  }
}

/**
 * Handler to dirty the listing cache for:
 *  - Directory changes
 *  - .spec.ts changes
 *  - README.txt changes
 * Also dirties the compile cache for changed files.
 */
function dirtyListingAndCompileCache(absPath, stats) {
  const relPath = path.relative(srcDir, absPath);

  const segments = relPath.split(path.sep);
  // The listing changes if the directories change, or if a .spec.ts file is added/removed.
  const listingChange =
  // A directory or a file with no extension that we can't stat.
  // (stat doesn't work for deletions)
  (path.extname(relPath) === '' && (stats === undefined || !stats.isFile()) ||
  // A spec file
  relPath.endsWith('.spec.ts') ||
  // A README.txt
  path.basename(relPath, 'txt') === 'README') &&
  segments.length > 0;
  if (listingChange) {
    const suite = segments[0];
    if (listingCache.has(suite)) {
      console.debug('Dirtying listing cache', suite);
    }
    listingCache.delete(suite);
  }

  dirtyCompileCache(absPath, stats);
}

watcher.on('add', dirtyListingAndCompileCache);
watcher.on('unlink', dirtyListingAndCompileCache);
watcher.on('addDir', dirtyListingAndCompileCache);
watcher.on('unlinkDir', dirtyListingAndCompileCache);
watcher.on('change', dirtyCompileCache);

const app = express();

// Send Chrome Origin Trial tokens
app.use((req, res, next) => {
  res.header('Origin-Trial', [
  // Token for http://localhost:8080
  'AvyDIV+RJoYs8fn3W6kIrBhWw0te0klraoz04mw/nPb8VTus3w5HCdy+vXqsSzomIH745CT6B5j1naHgWqt/tw8AAABJeyJvcmlnaW4iOiJodHRwOi8vbG9jYWxob3N0OjgwODAiLCJmZWF0dXJlIjoiV2ViR1BVIiwiZXhwaXJ5IjoxNjYzNzE4Mzk5fQ==']);

  next();
});

// Set up logging
app.use(morgan('dev'));

// Serve the standalone runner directory
app.use('/standalone', express.static(path.resolve(srcDir, '../standalone')));
// Add out-wpt/ build dir for convenience
app.use('/out-wpt', express.static(path.resolve(srcDir, '../out-wpt')));
app.use('/docs/tsdoc', express.static(path.resolve(srcDir, '../docs/tsdoc')));

// Serve a suite's listing.js file by crawling the filesystem for all tests.
app.get('/out/:suite/listing.js', async (req, res, next) => {
  const suite = req.params['suite'];

  if (listingCache.has(suite)) {
    res.setHeader('Content-Type', 'application/javascript');
    res.send(listingCache.get(suite));
    return;
  }

  try {
    const listing = await makeListing(path.resolve(srcDir, suite, 'listing.ts'));
    const result = `export const listing = ${JSON.stringify(listing, undefined, 2)}`;

    listingCache.set(suite, result);
    res.setHeader('Content-Type', 'application/javascript');
    res.send(result);
  } catch (err) {
    next(err);
  }
});

// Serve all other .js files by fetching the source .ts file and compiling it.
app.get('/out/**/*.js', async (req, res, next) => {
  const jsUrl = path.relative('/out', req.url);
  const tsUrl = jsUrl.replace(/\.js$/, '.ts');
  if (compileCache.has(tsUrl)) {
    res.setHeader('Content-Type', 'application/javascript');
    res.send(compileCache.get(tsUrl));
    return;
  }

  let absPath = path.join(srcDir, tsUrl);
  if (!fs.existsSync(absPath)) {
    // The .ts file doesn't exist. Try .js file in case this is a .js/.d.ts pair.
    absPath = path.join(srcDir, jsUrl);
  }

  try {
    const result = await babel.transformFileAsync(absPath, babelConfig);
    if (result && result.code) {
      compileCache.set(tsUrl, result.code);

      res.setHeader('Content-Type', 'application/javascript');
      res.send(result.code);
    } else {
      throw new Error(`Failed compile ${tsUrl}.`);
    }
  } catch (err) {
    next(err);
  }
});

const host = '0.0.0.0';
const port = 8080;
// Find an available port, starting at 8080.
portfinder.getPort({ host, port }, (err, port) => {
  if (err) {
    throw err;
  }
  watcher.on('ready', () => {
    // Listen on the available port.
    app.listen(port, host, () => {
      console.log('Standalone test runner running at:');
      for (const iface of Object.values(os.networkInterfaces())) {
        for (const details of iface || []) {
          if (details.family === 'IPv4') {
            console.log(`  http://${details.address}:${port}/standalone/`);
          }
        }
      }
    });
  });
});

// Serve everything else (not .js) as static, and directories as directory listings.
app.use('/out', serveIndex(path.resolve(srcDir, '../src')));
app.use('/out', express.static(path.resolve(srcDir, '../src')));
//# sourceMappingURL=dev_server.js.map