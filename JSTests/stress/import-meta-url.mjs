if (!import.meta.url.match("import-meta-url.mjs"))
    throw new Error("import.meta.url doesn't seem right: " + import.meta.url);
