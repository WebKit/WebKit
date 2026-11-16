//@ memoryHog!
//@ skip if $memoryLimited
//@ runDefaultWasm

// PGlite (PostgreSQL 17.5 compiled to wasm via Emscripten) running in the jsc shell.
// This is a heavy real-world wasm module (~8.5 MB, ~11k functions, dynamic linking,
// setjmp/longjmp, memory growth) — a good integration stress test for the wasm
// pipeline: compilation, IPInt/BBQ/OMG tier-up, indirect calls, traps, and GC.
//
// The test cases below are ported from Bun's pglite test suite
// (test/js/third_party/@electric-sql/pglite/pglite.test.ts and
//  test/js/third_party/pg-gateway/pglite.test.ts), driving the wasm module
// directly via the PostgreSQL wire protocol instead of through Bun's networking
// stack. See resources/pglite/jsc-harness.js for the shim layer.

load("./resources/pglite/jsc-harness.js");

function shouldBe(actual, expected, msg) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function shouldThrow(fn, code, msg) {
    let threw = false;
    try { fn(); } catch (e) { threw = true; if (code && e.code !== code) throw new Error(`${msg}: expected code ${code}, got ${e.code} (${e.message})`); }
    if (!threw) throw new Error(`${msg}: expected to throw`);
}

asyncTestStart(1);

(async () => {
    const db = await startPGlite("./resources/pglite");

    // --- Bun test 1: pglite.test.ts "can initialize successfully" ---
    {
        const { rows, fields } = db.query("SELECT version();");
        shouldBe(fields, [{ name: "version", dataTypeID: 25 }], "version() field");
        if (!/^PostgreSQL 17\.\d+ on \S+, compiled by emcc /.test(rows[0][0]))
            throw new Error("Unexpected version(): " + rows[0][0]);
    }

    // --- Bun test 2: pg-gateway/pglite.test.ts (wasm-relevant parts) ---
    // Set up the same schema and data the Bun test uses.
    db.query(`
        CREATE TABLE IF NOT EXISTS test_table (
            id SERIAL PRIMARY KEY,
            name TEXT NOT NULL
        );
        INSERT INTO test_table (name) VALUES ('Test 1'), ('Test 2'), ('Test 3');
    `);

    // "prepared statement without parameters" — extended query protocol, no params.
    {
        const { rows } = db.queryParams("SELECT * FROM test_table WHERE id = 1");
        shouldBe(rows, [["1", "Test 1"]], "prepared no-param");
    }

    // "using prepared statement" — extended query protocol with a parameter.
    {
        const { rows } = db.queryParams("SELECT * FROM test_table WHERE id = $1", [1]);
        shouldBe(rows, [["1", "Test 1"]], "prepared with param");
    }

    // "using simple query" — simple query protocol.
    {
        const { rows } = db.query("SELECT * FROM test_table WHERE id = 1");
        shouldBe(rows, [["1", "Test 1"]], "simple query");
    }

    // "using unsafe with parameters" — same wire protocol path as queryParams.
    {
        const { rows } = db.queryParams("SELECT * FROM test_table WHERE id = $1", ["1"]);
        shouldBe(rows, [["1", "Test 1"]], "unsafe with param");
    }

    // Extra coverage: verify ordering / multi-row / aggregates / errors.
    {
        const { rows } = db.query("SELECT id, name FROM test_table ORDER BY id DESC");
        shouldBe(rows, [["3", "Test 3"], ["2", "Test 2"], ["1", "Test 1"]], "order by desc");
    }
    {
        const { rows } = db.query("SELECT count(*)::int, min(id)::int, max(id)::int FROM test_table");
        shouldBe(rows, [["3", "1", "3"]], "aggregates");
    }
    {
        // Errors propagate through the wire protocol with a SQLSTATE code.
        shouldThrow(() => db.query("SELECT * FROM no_such_table"), "42P01", "undefined table");
        // Backend recovers after an error.
        const { rows } = db.query("SELECT 1::int");
        shouldBe(rows, [["1"]], "recover after error");
    }

    db.close();
    asyncTestPassed();
})().catch((e) => {
    print("FAIL: " + e + "\n" + (e && e.stack || ""));
    // Fall through without calling asyncTestPassed() => non-zero exit.
});
