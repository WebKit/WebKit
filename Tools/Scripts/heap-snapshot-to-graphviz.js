#!/usr/bin/env bun
// Usage: Tools/Scripts/heap-snapshot-to-graphviz.js snapshot output
// Produces output/GCDebugging.gv.txt, which you can open with GraphVIZ:
//      dot -Tsvg ~/GCDebugging.gv.txt -O
// You will probably want to edit this script manually to adjust the layout.
import { parseArgs } from "util"
import { file } from "bun"

let HeapSnapshot = {}
let HeapSnapshotDiff = {}
const nodeFieldCount = 4;
const nodeIdOffset = 0;
const nodeSizeOffset = 1;
const nodeClassNameOffset = 2;
const nodeFlagsOffset = 3;
const gcDebuggingNodeFieldCount = 7;

// node flags
const internalFlagsMask = (1 << 0);
const objectTypeMask = (1 << 1);

// edges
// [<0:fromId>, <1:toId>, <2:typeTableIndex>, <3:edgeDataIndexOrEdgeNameIndex>]
const edgeFieldCount = 4;
const edgeFromIdOffset = 0;
const edgeToIdOffset = 1;
const edgeTypeOffset = 2;
const edgeDataOffset = 3;

const rootFieldCount = 3;
const rootNodeIdOffset = 0;
const rootReasonIndexOffset = 1;

// Other constants.
const rootNodeIndex = 0;
const rootNodeOrdinal = 0;
const rootNodeIdentifier = 0;
eval(await file('Source/WebInspectorUI/UserInterface/Workers/HeapSnapshot/HeapSnapshot.js').text())

const { values, positionals } = parseArgs({
    args: Bun.argv,
    options: {
    },
    strict: true,
    allowPositionals: true,
  })

if (positionals.length != 4)
    throw "Usage: Tools/Scripts/heap-snapshot-to-graphviz.js snapshot output"

let fileName = positionals[2]
let outputName = positionals[3]
let json = await file(fileName).text()

let snapshot = new HeapSnapshot(0, json, "Snapshot", true)
console.log("Snapshot loaded")

function escapeOutput(unsafe) {
    return unsafe.replace(/["]/g, function (c) {
        switch (c) {
            case '"': return '-'
        }
    })
}

let output = `digraph Heap { 
 fontname="Helvetica,Arial,sans-serif";
 node [fontname="Helvetica,Arial,sans-serif"];
 edge [fontname="Helvetica,Arial,sans-serif"]; ` +

 /*`
 layout=twopi;
 ranksep=2;
 ratio=auto;
 overlap=false;
 overlap_scaling=100;
`*/

`
layout=neato
center=""
overlap=false
`

output += `
    n0[label="Root"];`

let {nodeOrdinalToPostOrderIndex, postOrderIndexToNodeOrdinal} = snapshot._buildPostOrderIndexes()

const rootPostOrderIndex = snapshot._nodeCount - 1;
for (let postOrderIndex = rootPostOrderIndex - 1; postOrderIndex >= 0; --postOrderIndex) {
    let {
        id,
        className,
        size,
        retainedSize,
        internal,
        isObjectType,
        gcRoot,
        dead,
        dominatorNodeIdentifier,
        hasChildren,
    } = snapshot.serializeNode(postOrderIndexToNodeOrdinal[postOrderIndex] * snapshot._nodeFieldCount)

    output += `
        n${id}[label=" ${escapeOutput(className)}" tooltip="${size}"];`
}

for (let edgeIndex = 0; edgeIndex < snapshot._edges.length; edgeIndex += edgeFieldCount) {
    let {
        from, to, type, data
    } = snapshot.serializeEdge(edgeIndex)

    output += `
        n${from} -> n${to} [label=" ${escapeOutput(type + ":" + data)}"];`
}

for (let nodeIndex = 0; nodeIndex < snapshot._nodes.length; nodeIndex += snapshot._nodeFieldCount) {
    let {
        id,
        className,
        size,
        retainedSize,
        internal,
        isObjectType,
        gcRoot,
        dead,
        dominatorNodeIdentifier,
        hasChildren,
    } = snapshot.serializeNode(nodeIndex)

    if (!gcRoot) {
        continue
    }

    let rootReason = ""
    if (gcRoot) {
        for (let rootIndex = 0; rootIndex < snapshot._roots.length; rootIndex += rootFieldCount) {
            let nodeID = snapshot._roots[rootIndex + rootNodeIdOffset]
            let reason = snapshot._labels[snapshot._roots[rootIndex + rootReasonIndexOffset]]
            if (nodeID != id)
                continue
            rootReason = reason
            break
        }
    }
    output += `
        n0 -> n${id} [label=" ROOT: ${escapeOutput(rootReason)}"];`
}

output += `
}
`

if (true) {
    await Bun.write(outputName + "/GCDebugging.gv.txt", output);
}

if (true) {
    let id = 276208
    console.log(snapshot.shortestGCRootPath(id))
    console.log("----")
    console.log(snapshot._determineGCRootPaths(id))
}