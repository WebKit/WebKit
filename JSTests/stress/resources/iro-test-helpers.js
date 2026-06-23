// Helpers for asserting facts produced by the DFG IntegerRangeOptimization
// phase (IRO). Tests pin a specific DFG node by wrapping its JS value in a
// `$vm.probe(id, value)` call: in DFG/FTL the call becomes a DebugProbe DFG
// node carrying that string id, and IRO emits a dump entry keyed by id.
//
// Requires --useTestingHelpers=1 --useDollarVM=1 --useConcurrentJIT=0
// ($vm.iroFactDump release-asserts concurrent JIT is off).
//
// ---- API ------------------------------------------------------------------
//
//   makeIROHelper(fn) → {
//       probes,                              // Map<id, probeEntry>
//       range(id),                           // { min, max } | null
//       rangeOf(atId, ofId),                 // range of probe `ofId` at probe `atId`'s point
//       relsAt(id),                          // relations recorded at probe `id`
//       assertRel({ at, lhs, rel, rhs, offset }),
//       assertNoRel({ at, with }),
//       assertEliminated(op, probeId),      // IRO removed `op` on that probe
//       assertNotEliminated(op, probeId),
//       opCount(op),                         // # of nodes with `op` IRO left
//       dfgGraph,                            // textual DFG dump (string)
//   }
//
// A probe entry has shape:
//   { id, range: {min,max}|null,
//     lineRanges: Map<otherProbeId, {min,max}|null>,
//     rels:       [ factEntry ] }   // facts that mention the probe
//
// A factEntry is:
//   { lhs, rhs, rel, offset }   // lhs/rhs is a probe id (string) or int constant (number)
//
// Relations: `lhs` / `rhs` for `assertRel` and `assertNoRel` accept a probe
// id (string), an integer constant (use { const: N }), or ANY (the Symbol).
// `rel` is one of "<", ">", "==", "!=", "<=", ">=". `<=` and `>=` are
// normalized to `<` / `>` with an adjusted offset.
//
// Matching is logical, not textual: a recorded `lhs > rhs + 5` matches a
// query for `lhs > rhs + 0`; `lhs == rhs + 3` matches `lhs > rhs + 2`; and a
// recorded `rhs < lhs - 5` matches a query for `lhs > rhs + 4` (orientation
// reversal). Constants on the RHS of a recorded fact match constant queries
// too.

const ANY = Symbol("IRO.ANY");

function makeIROHelper(fn) {
    // $vm.iroFactDump returns the fact object directly (probes + graph, with the
    // accumulated probe-pinned eliminations stitched on by $vm), or an empty
    // string if the function isn't FTL-compiled.
    const parsed = $vm.iroFactDump(fn);
    if (!parsed || typeof parsed !== "object")
        throw new Error("makeIROHelper: $vm.iroFactDump returned no fact object — was the function FTL-compiled?");

    // IR modifications IRO made to a probed value, e.g. eliminating a
    // CheckInBounds whose index is a probe. Keyed "op|probeId" so a test can
    // assert a specific removal happened (or didn't) on a specific probe,
    // independent of graph-wide op counts.
    const elimSet = new Set((parsed.eliminations || []).map(e => e.op + "|" + e.on));

    // opCount(op) counts `Op(` node headers in the dump. The trailing `(` makes
    // it exact (operands are printed as D@N, and it won't match a longer op).
    const dfgGraph = typeof parsed.graph === "string" ? parsed.graph : "";
    function opCount(op) {
        if (!dfgGraph)
            return 0;
        const escaped = op.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
        const matches = dfgGraph.match(new RegExp("\\b" + escaped + "\\(", "g"));
        return matches ? matches.length : 0;
    }

    // IRO emits each fact in canonical form (anchor on the left) but may repeat
    // it: a probe's id is shared by the DebugProbe node and the value it wraps,
    // and IRO stores every relationship in both orientations. Canonical form
    // means equal facts stringify identically, so a textual key dedupes them.
    function dedupeFacts(rels) {
        const seen = new Set();
        return rels.filter(f => {
            const key = [f.lhs, f.rel, f.rhs, f.offset].join("|");
            return seen.has(key) ? false : (seen.add(key), true);
        });
    }

    const probes = new Map();
    for (const raw of (parsed.probes || [])) {
        const lineRanges = new Map();
        for (const lr of (raw.lineRanges || [])) {
            lineRanges.set(lr.id, lr.range ? { min: lr.range[0], max: lr.range[1] } : null);
        }
        probes.set(raw.id, {
            id: raw.id,
            range: raw.range ? { min: raw.range[0], max: raw.range[1] } : null,
            lineRanges,
            rels: dedupeFacts(raw.rels || []),
        });
    }

    function getProbe(id, role) {
        if (typeof id !== "string")
            throw new Error("makeIROHelper: " + role + " must be a probe id (string); got " + typeof id);
        const p = probes.get(id);
        if (!p)
            throw new Error("makeIROHelper: no probe with id " + JSON.stringify(id)
                + " found in dump (known ids: "
                + [...probes.keys()].map(JSON.stringify).join(", ") + ")");
        return p;
    }

    function labelOf(id) {
        return JSON.stringify(id);
    }

    function formatRange(r) {
        return r ? (r.min + ".." + r.max) : "none";
    }

    function wrapRange(raw, label) {
        if (raw == null) return null;
        const r = { min: raw.min, max: raw.max };
        r.toString = function() {
            return "range " + formatRange(this) + " (" + label + ")";
        };
        return r;
    }

    // The relsAt(id) result carries its facts plus the per-probe ranges that
    // are visible at this anchor, so the printout shows the range of every
    // probe mentioned in the facts (it's confusing to read a fact like
    //   x-inside < bitor-inside + 1
    // without seeing x-inside's range at the same point).
    function wrapRels(probe) {
        const arr = probe.rels.slice();
        arr.toString = function() {
            const lines = [];
            lines.push("Relations at " + labelOf(probe.id) + " (" + this.length + "):");

            // Collect ranges for every probe mentioned in any fact (plus the
            // anchor itself). The anchor's own range comes from `probe.range`;
            // the other probes' ranges live in `probe.lineRanges` (a map of
            // probeId → range as recorded at this anchor's IR position).
            const referenced = new Set();
            referenced.add(probe.id);
            for (const f of this) {
                if (typeof f.lhs === "string") referenced.add(f.lhs);
                if (typeof f.rhs === "string") referenced.add(f.rhs);
            }
            const rangesForLabel = new Map();
            rangesForLabel.set(probe.id, probe.range);
            for (const otherId of referenced) {
                if (otherId === probe.id) continue;
                if (probe.lineRanges.has(otherId))
                    rangesForLabel.set(otherId, probe.lineRanges.get(otherId));
            }
            if (rangesForLabel.size > 0) {
                lines.push("  ranges:");
                for (const [id, r] of rangesForLabel)
                    lines.push("    " + labelOf(id) + ": " + formatRange(r));
            }

            if (this.length === 0)
                lines.push("  facts: (no relations recorded)");
            else {
                lines.push("  facts:");
                for (const f of this)
                    lines.push("    " + describeFact(f));
            }
            return lines.join("\n");
        };
        return arr;
    }

    function range(id) {
        return wrapRange(getProbe(id, "id").range, labelOf(id));
    }

    function rangeOf(atId, ofId) {
        const at = getProbe(atId, "atId");
        if (atId === ofId)
            return wrapRange(at.range, labelOf(ofId) + " at " + labelOf(atId));
        const raw = at.lineRanges.has(ofId) ? at.lineRanges.get(ofId) : null;
        return wrapRange(raw, labelOf(ofId) + " at " + labelOf(atId));
    }

    function relsAt(id) {
        return wrapRels(getProbe(id, "id"));
    }

    // Canonicalize a query relation. IRO records only `<`, `>`, `==`, `!=`;
    // `<=` and `>=` are expressed via an off-by-one in the offset.
    function canonicalRelation(rel, offset) {
        if (rel === ">=") return { rel: ">", offset: offset - 1 };
        if (rel === "<=") return { rel: "<", offset: offset + 1 };
        if (rel === ">" || rel === "<" || rel === "==" || rel === "!=")
            return { rel, offset };
        throw new Error("makeIROHelper: unsupported relation '" + rel + "'");
    }
    function flipRel(rel) {
        if (rel === "<") return ">";
        if (rel === ">") return "<";
        return rel;
    }

    // Normalize a user-supplied operand to a triple { probeId?, constant?, any }.
    function normalizeOperand(operand, role) {
        if (operand === ANY)
            return { any: true };
        if (typeof operand === "string")
            return { probeId: operand };
        if (operand && typeof operand === "object" && typeof operand.const === "number")
            return { constant: operand.const };
        throw new Error("makeIROHelper: " + role + " must be a probe id (string), "
            + "{ const: N }, or iro.ANY");
    }

    // For a recorded fact's side, normalize to the same shape so we can
    // compare.
    function recordedSide(facta, prefix) {
        const v = facta[prefix];
        if (typeof v === "string") return { probeId: v };
        if (typeof v === "number") return { constant: v };
        return {};
    }
    function sideMatches(recorded, query) {
        if (query.any) return true;
        if (query.probeId != null) return recorded.probeId === query.probeId;
        if (query.constant != null) return recorded.constant === query.constant;
        return false;
    }

    function impliedBy(fRel, fOffset, qRel, qOffset) {
        if (fRel === ">") {
            if (qRel === ">")  return fOffset >= qOffset;
            if (qRel === "!=") return fOffset >= qOffset;
            return false;
        }
        if (fRel === "<") {
            if (qRel === "<")  return fOffset <= qOffset;
            if (qRel === "!=") return fOffset <= qOffset;
            return false;
        }
        if (fRel === "==") {
            if (qRel === "==") return fOffset === qOffset;
            if (qRel === ">")  return fOffset > qOffset;
            if (qRel === "<")  return fOffset < qOffset;
            if (qRel === "!=") return fOffset !== qOffset;
            return false;
        }
        if (fRel === "!=") {
            return qRel === "!=" && fOffset === qOffset;
        }
        return false;
    }
    function factImplies(f, q) {
        const fLhs = recordedSide(f, "lhs");
        const fRhs = recordedSide(f, "rhs");
        if (sideMatches(fLhs, q.lhs) && sideMatches(fRhs, q.rhs)
                && impliedBy(f.rel, f.offset, q.rel, q.offset))
            return true;
        if (sideMatches(fLhs, q.rhs) && sideMatches(fRhs, q.lhs)
                && impliedBy(flipRel(f.rel), -f.offset, q.rel, q.offset))
            return true;
        return false;
    }

    function describeFact(f) {
        const fmt = (prefix) => {
            const v = f[prefix];
            if (typeof v === "string") return JSON.stringify(v);
            if (typeof v === "number") return String(v);
            return "?";
        };
        let s = fmt("lhs") + f.rel + fmt("rhs");
        if (f.offset > 0) s += "+" + f.offset;
        else if (f.offset < 0) s += "-" + (-f.offset);
        return s;
    }
    function describeSide(s) {
        if (s.any) return "ANY";
        if (s.probeId != null) return JSON.stringify(s.probeId);
        if (s.constant != null) return String(s.constant);
        return "?";
    }

    function assertRel(spec) {
        if (typeof spec.at !== "string")
            throw new Error("assertRel: { at } must be a probe id (string)");
        if (spec.rel == null)
            throw new Error("assertRel: { rel } is required");
        const anchor = getProbe(spec.at, "at");
        const lhs = normalizeOperand(spec.lhs == null ? spec.at : spec.lhs, "lhs");
        const rhs = normalizeOperand(spec.rhs == null ? ANY : spec.rhs, "rhs");
        const rawOffset = spec.offset == null ? 0 : spec.offset;
        const { rel: qRel, offset: qOffset } = canonicalRelation(spec.rel, rawOffset);
        const q = { lhs, rhs, rel: qRel, offset: qOffset };
        for (const f of anchor.rels) {
            if (factImplies(f, q))
                return f;
        }
        const recorded = anchor.rels.length === 0
            ? "    (no relations recorded at this probe)"
            : anchor.rels.map(f => "    " + describeFact(f)).join("\n");
        throw new Error("assertRel: no recorded fact implies "
            + describeSide(lhs) + spec.rel + describeSide(rhs)
            + (rawOffset ? (rawOffset > 0 ? "+" + rawOffset : String(rawOffset)) : "")
            + " at " + labelOf(spec.at) + "\n  recorded facts:\n" + recorded);
    }

    function assertNoRel(spec) {
        if (typeof spec.at !== "string")
            throw new Error("assertNoRel: { at } must be a probe id (string)");
        if (spec.with === undefined)
            throw new Error("assertNoRel: { with } is required (probe id, { const: N }, or iro.ANY)");
        const anchor = getProbe(spec.at, "at");
        const selfQ = { probeId: spec.at };
        const otherQ = normalizeOperand(spec.with, "with");
        for (const f of anchor.rels) {
            const lhs = recordedSide(f, "lhs");
            const rhs = recordedSide(f, "rhs");
            const involvesSelf = sideMatches(lhs, selfQ) || sideMatches(rhs, selfQ);
            if (!involvesSelf) continue;
            if (otherQ.any) {
                // Filter out the trivial self==self echo (probe == probe + 0).
                const isSelfEcho =
                    sideMatches(lhs, selfQ) && sideMatches(rhs, selfQ)
                    && f.rel === "==" && f.offset === 0;
                if (isSelfEcho) continue;
                throw new Error("assertNoRel: recorded relation " + describeFact(f)
                    + " involves " + labelOf(spec.at));
            }
            const involvesOther = sideMatches(lhs, otherQ) || sideMatches(rhs, otherQ);
            if (involvesOther)
                throw new Error("assertNoRel: recorded relation " + describeFact(f)
                    + " links " + labelOf(spec.at) + " with " + describeSide(otherQ));
        }
    }

    // Assert IRO recorded (or did not record) applying `op` to the value
    // pinned by probe `probeId` — e.g. assertEliminated("CheckInBounds", "i").
    function assertEliminated(op, probeId) {
        if (!elimSet.has(op + "|" + probeId))
            throw new Error("assertEliminated: IRO did not eliminate " + op
                + " on probe " + JSON.stringify(probeId) + " (recorded: ["
                + [...elimSet].join(", ") + "])");
    }
    function assertNotEliminated(op, probeId) {
        if (elimSet.has(op + "|" + probeId))
            throw new Error("assertNotEliminated: IRO eliminated " + op
                + " on probe " + JSON.stringify(probeId) + ", but the test expected it kept");
    }

    return {
        ANY,
        probes,
        range,
        rangeOf,
        relsAt,
        assertRel,
        assertNoRel,
        assertEliminated,
        assertNotEliminated,
        opCount,
        dfgGraph,
    };
}
