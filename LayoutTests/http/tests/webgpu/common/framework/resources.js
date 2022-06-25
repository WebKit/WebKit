/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /**
 * Base path for resources. The default value is correct for non-worker WPT, but standalone and
 * workers must access resources using a different base path, so this is overridden in
 * `test_worker-worker.ts` and `standalone.ts`.
 */ let baseResourcePath = './resources';
/**
 * Get a path to a resource in the `resources` directory, relative to the current execution context
 * (html file or worker .js file), for `fetch()`, `<img>`, `<video>`, etc.
 */
export function getResourcePath(pathRelativeToResourcesDir) {
  return baseResourcePath + '/' + pathRelativeToResourcesDir;
}

/**
 * Set the base resource path (path to the `resources` directory relative to the current
 * execution context).
 */
export function setBaseResourcePath(path) {
  baseResourcePath = path;
}
