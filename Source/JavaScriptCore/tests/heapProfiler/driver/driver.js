function assert(condition, reason) {
    if (!condition)
        throw new Error(reason);
}

// -----------------
// CheapHeapSnapshot
//
// Contains two large lists of all node data and all edge data.
// Lazily creates node and edge objects off of indexes into these lists.

// [<0:id>, <1:size>, <2:classNameTableIndex>, <3:internal>, <4:firstEdgeIndex>];
const nodeFieldCount = 5;
const nodeIdOffset = 0;
const nodeSizeOffset = 1;
const nodeClassNameOffset = 2;
const nodeInternalOffset = 3;
const nodeFirstEdgeOffset = 4;
const nodeNoEdgeValue = 0xffffffff; // UINT_MAX

// [<0:from-id>, <1:to-id>, <2:typeTableIndex>, <3:data>]
const edgeFieldCount = 4;
const edgeFromIdOffset = 0;
const edgeToIdOffset = 1;
const edgeTypeOffset = 2;
const edgeDataOffset = 3;

CheapHeapSnapshotNode = class CheapHeapSnapshotNode
{
    constructor(snapshot, nodeIndex)
    {
        assert((nodeIndex % nodeFieldCount) === 0, "Bad Node Index: " + nodeIndex);

        let nodes = snapshot.nodes;
        this.id = nodes[nodeIndex + nodeIdOffset];
        this.size = nodes[nodeIndex + nodeSizeOffset];
        this.className = snapshot.classNameFromTableIndex(nodes[nodeIndex + nodeClassNameOffset]);
        this.internal = nodes[nodeIndex + nodeInternalOffset] ? true : false;

        this.outgoingEdges = [];
        let firstEdgeIndex = nodes[nodeIndex + nodeFirstEdgeOffset];
        if (firstEdgeIndex !== nodeNoEdgeValue) {
            for (let i = firstEdgeIndex; i < snapshot.edges.length; i += edgeFieldCount) {
                if (snapshot.edges[i + edgeFromIdOffset] !== this.id)
                    break;
                this.outgoingEdges.push(new CheapHeapSnapshotEdge(snapshot, i));
            }
        }
    }
}

CheapHeapSnapshotEdge = class CheapHeapSnapshotEdge
{
    constructor(snapshot, edgeIndex)
    {
        assert((edgeIndex % edgeFieldCount) === 0, "Bad Edge Index: " + edgeIndex);
        this.snapshot = snapshot;

        let edges = snapshot.edges;
        this.fromId = edges[edgeIndex + edgeFromIdOffset];
        this.toId = edges[edgeIndex + edgeToIdOffset];
        this.type = snapshot.edgeTypeFromTableIndex(edges[edgeIndex + edgeTypeOffset]);
        this.data = edges[edgeIndex + edgeDataOffset];
    }

    get from() { return this.snapshot.nodeWithIdentifier(this.fromId); }
    get to() { return this.snapshot.nodeWithIdentifier(this.toId); }
}

CheapHeapSnapshot = class CheapHeapSnapshot
{
    constructor(json)
    {
        let {nodes, nodeClassNames, edges, edgeTypes} = json;

        this._nodes = new Uint32Array(nodes.length * nodeFieldCount);
        this._edges = new Uint32Array(edges.length * edgeFieldCount);
        this._nodeIdentifierToIndex = new Map; // <id> => index in _nodes

        this._edgeTypesTable = edgeTypes;
        this._nodeClassNamesTable = nodeClassNames;

        let n = 0;
        nodes.forEach((nodePayload) => {
            let [id, size, classNameTableIndex, internal] = nodePayload;
            this._nodeIdentifierToIndex.set(id, n);
            this._nodes[n++] = id;
            this._nodes[n++] = size;
            this._nodes[n++] = classNameTableIndex;
            this._nodes[n++] = internal;
            this._nodes[n++] = nodeNoEdgeValue;
        });

        let e = 0;
        let lastNodeIdentifier = -1;
        edges.sort((a, b) => a[0] - b[0]).forEach((edgePayload) => {
            let [fromIdentifier, toIdentifier, edgeTypeTableIndex, data] = edgePayload;
            if (fromIdentifier !== lastNodeIdentifier) {
                let nodeIndex = this._nodeIdentifierToIndex.get(fromIdentifier);
                assert(this._nodes[nodeIndex + nodeIdOffset] === fromIdentifier, "Node lookup failed");
                this._nodes[nodeIndex + nodeFirstEdgeOffset] = e;
                lastNodeIdentifier = fromIdentifier;
            }
            this._edges[e++] = fromIdentifier;
            this._edges[e++] = toIdentifier;
            this._edges[e++] = edgeTypeTableIndex;
            this._edges[e++] = data;
        });
    }

    get nodes() { return this._nodes; }
    get edges() { return this._edges; }

    nodeWithIdentifier(id)
    {
        return new CheapHeapSnapshotNode(this, this._nodeIdentifierToIndex.get(id));
    }

    nodesWithClassName(className)
    {
        let result = [];
        for (let i = 0; i < this._nodes.length; i += nodeFieldCount) {
            let classNameTableIndex = this._nodes[i + nodeClassNameOffset];
            if (this.classNameFromTableIndex(classNameTableIndex) === className)
                result.push(new CheapHeapSnapshotNode(this, i));
        }
        return result;
    }

    classNameFromTableIndex(tableIndex)
    {
        return this._nodeClassNamesTable[tableIndex];
    }

    edgeTypeFromTableIndex(tableIndex)
    {
        return this._edgeTypesTable[tableIndex];
    }
}

function createCheapHeapSnapshot() {
    let json = generateHeapSnapshot();

    let {version, nodes, nodeClassNames, edges, edgeTypes} = json;
    assert(version === 1, "Heap Snapshot payload should be version 1");
    assert(nodes.length, "Heap Snapshot should have nodes");
    assert(nodeClassNames.length, "Heap Snapshot should have nodeClassNames");
    assert(edges.length, "Heap Snapshot should have edges");
    assert(edgeTypes.length, "Heap Snapshot should have edgeTypes");

    return new CheapHeapSnapshot(json);
}


// ------------
// HeapSnapshot
//
// This creates a lot of objects that make it easy to walk the entire node graph
// (incoming and outgoing edges). However when a test creates multiple snapshots
// with snapshots in scope this can quickly explode into a snapshot with a massive
// number of nodes/edges. For such cases create CheapHeapSnapshots, which create
// a very small number of objects per Heap Snapshot.

HeapSnapshotNode = class HeapSnapshotNode
{
    constructor(id, className, size, internal)
    {
        this.id = id;
        this.className = className;
        this.size = size; 
        this.internal = internal;
        this.incomingEdges = [];
        this.outgoingEdges = [];
    }
}

HeapSnapshotEdge = class HeapSnapshotEdge
{
    constructor(from, to, type, data)
    {
        this.from = from;
        this.to = to;
        this.type = type;
        this.data = data;
    }
}

HeapSnapshot = class HeapSnapshot
{
    constructor(json)
    {        
        let {version, nodes, nodeClassNames, edges, edgeTypes} = json;

        this.nodeMap = new Map;

        this.nodes = nodes.map((nodePayload) => {
            let [id, size, classNameIndex, internal] = nodePayload;
            let node = new HeapSnapshotNode(id, nodeClassNames[classNameIndex], size, internal);
            this.nodeMap.set(id, node);
            return node;
        });

        edges.map((edgePayload) => {
            let [fromIdentifier, toIdentifier, edgeTypeIndex, data] = edgePayload;
            let from = this.nodeMap.get(fromIdentifier);
            let to = this.nodeMap.get(toIdentifier);
            assert(from, "Missing node for `from` part of edge");
            assert(to, "Missing node for `to` part of edge");
            let edge = new HeapSnapshotEdge(from, to, edgeTypes[edgeTypeIndex], data);
            from.outgoingEdges.push(edge);
            to.incomingEdges.push(edge);
        });

        this.rootNode = this.nodeMap.get(0);
        assert(this.rootNode, "Missing <root> node with identifier 0");
        assert(this.rootNode.outgoingEdges.length > 0, "<root> should have children");
        assert(this.rootNode.incomingEdges.length === 0, "<root> should not have back references");
    }

    nodesWithClassName(className)
    {
        let result = [];
        for (let node of this.nodes) {
            if (node.className === className)
                result.push(node);
        }
        return result;
    }
}

function createHeapSnapshot() {
    let json = generateHeapSnapshot();

    let {version, nodes, nodeClassNames, edges, edgeTypes} = json;
    assert(version === 1, "Heap Snapshot payload should be version 1");
    assert(nodes.length, "Heap Snapshot should have nodes");
    assert(nodeClassNames.length, "Heap Snapshot should have nodeClassNames");
    assert(edges.length, "Heap Snapshot should have edges");
    assert(edgeTypes.length, "Heap Snapshot should have edgeTypes");

    return new HeapSnapshot(json);
}
