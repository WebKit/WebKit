// PGlite (PostgreSQL compiled to wasm) test harness for the jsc shell.
//
// Vendored from npm @electric-sql/pglite@0.3.15 (Apache-2.0, see LICENSE).
// pglite.js / pglite.wasm / pglite.data are unmodified. This harness drives the
// raw Emscripten module directly: we don't use the PGlite TypeScript API
// (index.js) because its dynamic-import / fetch / Worker / IndexedDB
// abstractions are not available in the shell. Instead we configure the
// Emscripten Module hooks ourselves and speak the PostgreSQL wire protocol
// directly via the `_set_read_write_cbs` / `_interactive_one` exports — the
// same mechanism index.js uses internally.

// --- Polyfills the prebuilt Emscripten glue expects ---
// crypto.getRandomValues — used by Emscripten's initRandomFill. A deterministic
// PRNG is fine for tests (and makes runs reproducible).
if (typeof globalThis.crypto === "undefined") {
    let seed = 0x12345678;
    globalThis.crypto = {
        getRandomValues(view) {
            const bytes = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
            for (let i = 0; i < bytes.length; i++) {
                seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                bytes[i] = seed & 0xff;
            }
            return view;
        }
    };
}
if (typeof globalThis.console === "undefined" || globalThis.console === undefined) {
    const noop = () => {};
    globalThis.console = { log: noop, warn: noop, error: noop, info: noop, debug: noop, trace: noop, assert: noop };
}
if (typeof globalThis.queueMicrotask === "undefined")
    globalThis.queueMicrotask = (fn) => Promise.resolve().then(fn);

// --- PostgreSQL wire protocol message builders ---
const PGProtocol = {
    // Build a frontend message: tag byte + int32be(length incl. self) + body
    message(tag, body) {
        const len = 4 + body.length;
        const out = new Uint8Array(1 + 4 + body.length);
        out[0] = tag;
        out[1] = (len >>> 24) & 0xff; out[2] = (len >>> 16) & 0xff;
        out[3] = (len >>> 8) & 0xff; out[4] = len & 0xff;
        out.set(body, 5);
        return out;
    },
    cstr(s) {
        const out = new Uint8Array(s.length + 1);
        for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
        return out;
    },
    i16(n) { return new Uint8Array([(n >>> 8) & 0xff, n & 0xff]); },
    i32(n) { return new Uint8Array([(n >>> 24) & 0xff, (n >>> 16) & 0xff, (n >>> 8) & 0xff, n & 0xff]); },
    cat(...parts) {
        let total = 0;
        for (const p of parts) total += p.length;
        const out = new Uint8Array(total);
        let off = 0;
        for (const p of parts) { out.set(p, off); off += p.length; }
        return out;
    },
    // Q: simple query
    query(sql) { return this.message(0x51, this.cstr(sql)); },
    // P: parse — name, query, param type OIDs (0 = let server infer)
    parse(name, sql, paramTypes = []) {
        return this.message(0x50, this.cat(
            this.cstr(name), this.cstr(sql), this.i16(paramTypes.length),
            ...paramTypes.map(t => this.i32(t))));
    },
    // B: bind — portal, statement, param format codes, params (text), result format codes
    bind(portal, stmt, params = []) {
        const paramParts = [];
        for (const p of params) {
            if (p === null) { paramParts.push(this.i32(-1)); continue; }
            const bytes = this.cstr(String(p));
            paramParts.push(this.i32(bytes.length - 1), bytes.subarray(0, bytes.length - 1));
        }
        return this.message(0x42, this.cat(
            this.cstr(portal), this.cstr(stmt),
            this.i16(0),                       // 0 param format codes => all text
            this.i16(params.length), ...paramParts,
            this.i16(0)));                     // 0 result format codes => all text
    },
    // D: describe portal or statement
    describe(kind, name) { return this.message(0x44, this.cat(new Uint8Array([kind.charCodeAt(0)]), this.cstr(name))); },
    // E: execute portal, maxRows (0 = unlimited)
    execute(portal, maxRows = 0) { return this.message(0x45, this.cat(this.cstr(portal), this.i32(maxRows))); },
    // S: sync
    sync() { return this.message(0x53, new Uint8Array(0)); },
};

// Backend message parser. Returns { rows: string[][], fields: {name,dataTypeID}[], commandTags: string[] }.
function parseBackendMessages(bytes) {
    const rows = [], fields = [], commandTags = [];
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let i = 0;
    while (i < bytes.length) {
        const tag = String.fromCharCode(bytes[i]);
        const len = dv.getInt32(i + 1, false);
        const bodyStart = i + 5, bodyEnd = i + 1 + len;
        if (tag === "T") {
            let p = bodyStart;
            const n = dv.getInt16(p, false); p += 2;
            for (let f = 0; f < n; f++) {
                let end = p; while (bytes[end] !== 0) end++;
                let name = ""; for (let q = p; q < end; q++) name += String.fromCharCode(bytes[q]);
                p = end + 1;
                p += 4; p += 2;
                const dataTypeID = dv.getInt32(p, false); p += 4;
                p += 2; p += 4; p += 2;
                fields.push({ name, dataTypeID });
            }
        } else if (tag === "D") {
            let p = bodyStart;
            const n = dv.getInt16(p, false); p += 2;
            const row = [];
            for (let c = 0; c < n; c++) {
                const colLen = dv.getInt32(p, false); p += 4;
                if (colLen === -1) { row.push(null); continue; }
                let s = ""; for (let q = p; q < p + colLen; q++) s += String.fromCharCode(bytes[q]);
                p += colLen;
                row.push(s);
            }
            rows.push(row);
        } else if (tag === "C") {
            let s = ""; for (let q = bodyStart; q < bodyEnd && bytes[q] !== 0; q++) s += String.fromCharCode(bytes[q]);
            commandTags.push(s);
        } else if (tag === "E") {
            let p = bodyStart, msg = "", code = "";
            while (p < bodyEnd && bytes[p] !== 0) {
                const ft = String.fromCharCode(bytes[p]); p++;
                let end = p; while (bytes[end] !== 0) end++;
                let v = ""; for (let q = p; q < end; q++) v += String.fromCharCode(bytes[q]);
                p = end + 1;
                if (ft === "M") msg = v;
                if (ft === "C") code = v;
            }
            const e = new Error(`PostgreSQL error ${code}: ${msg}`);
            e.code = code;
            throw e;
        }
        i = bodyEnd;
    }
    return { rows, fields, commandTags };
}

async function startPGlite(resourceDir, opts = {}) {
    const wasmBytes = readFile(resourceDir + "/pglite.wasm", "binary");
    const dataBytes = readFile(resourceDir + "/pglite.data", "binary");
    // import() resolves relative to this file's URL, not cwd. pglite.js is a sibling.
    const PostgresMod = (await import("./pglite.js")).default;

    const PREFIX = "/tmp/pglite";   // where the Emscripten file_packager preamble unpacks pglite.data
    const PGDATA = "/pgdata";

    const mod = await PostgresMod({
        wasmBinary: wasmBytes,
        getPreloadedPackage: (name, size) =>
            dataBytes.buffer.slice(dataBytes.byteOffset, dataBytes.byteOffset + dataBytes.byteLength),
        locateFile: (path) => path,
        print: opts.print || (() => {}),
        printErr: opts.printErr || (() => {}),
        // PGlite passes its config through argv (its PostgreSQL fork parses these).
        arguments: [
            `PGDATA=${PGDATA}`, `PREFIX=${PREFIX}`,
            `PGUSER=${opts.username ?? "postgres"}`, `PGDATABASE=${opts.database ?? "template1"}`,
            "MODE=REACT", "REPL=N",
        ],
        WASM_PREFIX: PREFIX,
        noExitRuntime: true,
        preRun: [(m) => {
            m.FS.mkdirTree(PGDATA);
            // /dev/blob device used by PGlite for COPY ... FROM/TO blob streams. Provide a
            // minimal no-op version; tests don't exercise blob COPY.
            const dev = m.FS.makedev(64, 0);
            m.FS.registerDevice(dev, { open: () => {}, close: () => {}, read: () => 0,
                write: (s, b, o, l) => l, llseek: () => 0 });
            m.FS.mkdev("/dev/blob", dev);
        }],
    });

    // Wire-protocol I/O channel via Emscripten addFunction callbacks.
    // readCb(destPtr, max): JS -> wasm (request bytes).  writeCb(srcPtr, len): wasm -> JS (response bytes).
    let pendingReq = new Uint8Array(0), reqOff = 0, respChunks = [];
    const readCb = mod.addFunction((destPtr, max) => {
        const n = Math.min(pendingReq.length - reqOff, max);
        if (n > 0) { mod.HEAP8.set(pendingReq.subarray(reqOff, reqOff + n), destPtr); reqOff += n; }
        return n;
    }, "iii");
    const writeCb = mod.addFunction((srcPtr, len) => {
        respChunks.push(mod.HEAPU8.slice(srcPtr, srcPtr + len));
        return len;
    }, "iii");
    mod._set_read_write_cbs(readCb, writeCb);

    const initRes = mod._pgl_initdb();
    if (initRes & 1) throw new Error("INITDB: failed to execute");
    mod._pgl_backend();

    function execProtocolRaw(bytes) {
        pendingReq = bytes; reqOff = 0; respChunks = [];
        mod._interactive_one(bytes.length, bytes[0]);
        pendingReq = new Uint8Array(0); reqOff = 0;
        let total = 0; for (const c of respChunks) total += c.length;
        const out = new Uint8Array(total);
        let off = 0; for (const c of respChunks) { out.set(c, off); off += c.length; }
        return out;
    }

    // PGlite's TS wrapper does this after backend boot too.
    execProtocolRaw(PGProtocol.query("SET search_path TO public;"));

    return {
        mod,
        execProtocolRaw,
        // Simple query protocol.
        query(sql) {
            return parseBackendMessages(execProtocolRaw(PGProtocol.query(sql)));
        },
        // Extended query protocol with text parameters (Parse/Bind/Describe/Execute/Sync).
        queryParams(sql, params = []) {
            const req = PGProtocol.cat(
                PGProtocol.parse("", sql),
                PGProtocol.bind("", "", params),
                PGProtocol.describe("P", ""),
                PGProtocol.execute(""),
                PGProtocol.sync());
            return parseBackendMessages(execProtocolRaw(req));
        },
        close() {
            try { mod._pgl_shutdown(); } catch (e) {}
            mod.removeFunction(readCb);
            mod.removeFunction(writeCb);
        },
    };
}

globalThis.startPGlite = startPGlite;
globalThis.PGProtocol = PGProtocol;
globalThis.parseBackendMessages = parseBackendMessages;
