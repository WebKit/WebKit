// Tiny psql-like SQL REPL for verifying that PGlite (PostgreSQL on wasm) works
// in the jsc shell. Not a test — a manual sanity check / demo.
//
// Run from JSTests/wasm/stress/:
//   DYLD_FRAMEWORK_PATH=$BUILD/Release $BUILD/Release/jsc resources/pglite/psql.js
//
// Then type SQL at the `pglite=#` prompt. End each statement with `;`.
// Type `\q` to quit, `\d` to list tables.

load("./resources/pglite/jsc-harness.js");

function pad(s, w) { s = String(s ?? ""); return s.length >= w ? s : s + " ".repeat(w - s.length); }

function printTable({ rows, fields, commandTags }) {
    if (fields.length) {
        const widths = fields.map((f, i) => Math.max(f.name.length, ...rows.map(r => String(r[i] ?? "").length)));
        print(" " + fields.map((f, i) => pad(f.name, widths[i])).join(" | "));
        print("-" + widths.map(w => "-".repeat(w)).join("-+-") + "-");
        for (const row of rows)
            print(" " + row.map((c, i) => pad(c, widths[i])).join(" | "));
        print(`(${rows.length} row${rows.length === 1 ? "" : "s"})`);
    }
    for (const tag of commandTags)
        if (!fields.length || !/^SELECT/.test(tag)) print(tag);
}

(async () => {
    print("Booting PostgreSQL on WebAssembly in the jsc shell...");
    const t0 = performance.now();
    const db = await startPGlite("./resources/pglite");
    print(`Ready in ${(performance.now() - t0).toFixed(0)}ms.\n`);
    print(db.query("SELECT version();").rows[0][0]);
    print('\nType SQL terminated by ";" (or "\\d" for tables, "\\q" to quit).\n');

    let buf = "";
    while (true) {
        const prompt = buf ? "pglite-# " : "pglite=# ";
        // jsc's readline() doesn't support a prompt arg; print it ourselves without newline.
        // print() always appends \n, so the prompt sits on its own line — close enough.
        print(prompt);
        const line = readline();
        if (line === null || line === undefined) break;
        const trimmed = line.trim();
        if (!buf) {
            if (trimmed === "\\q" || trimmed === "exit" || trimmed === "quit") break;
            if (trimmed === "\\d") {
                printTable(db.query(
                    "SELECT table_name AS \"Table\", table_type AS \"Type\" FROM information_schema.tables " +
                    "WHERE table_schema = 'public' ORDER BY table_name;"));
                continue;
            }
            if (trimmed === "") continue;
        }
        buf += (buf ? "\n" : "") + line;
        if (!buf.trimEnd().endsWith(";")) continue;
        const sql = buf;
        buf = "";
        try {
            const t = performance.now();
            const result = db.query(sql);
            printTable(result);
            print(`Time: ${(performance.now() - t).toFixed(2)}ms\n`);
        } catch (e) {
            print("ERROR: " + (e.message || e) + "\n");
        }
    }
    db.close();
    print("Bye.");
})().catch(e => print("FATAL: " + e + "\n" + (e.stack || "")));
