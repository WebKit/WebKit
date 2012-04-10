var initialize_HeapSnapshotTest = function() {

InspectorTest.createHeapSnapshotMockObject = function()
{
    return {
        _rootNodeIndex: 0,
        _nodeTypeOffset: 0,
        _nodeNameOffset: 1,
        _edgesCountOffset: 2,
        _firstEdgeIndexOffset: 2,
        _firstEdgeOffset: 3,
        _nodeFieldCount: 3,
        _edgeFieldsCount: 3,
        _edgeTypeOffset: 0,
        _edgeNameOffset: 1,
        _edgeToNodeOffset: 2,
        _nodeTypes: ["hidden", "object"],
        _edgeTypes: ["element", "property"],
        _edgeShortcutType: -1,
        _edgeHiddenType: -1,
        _edgeElementType: 0,
        // Represents the following graph:
        //   (numbers in parentheses indicate node offset)
        // 
        //         A (3) --ac- C (9) -ce- E(15)
        //       a/|         /
        //  "" (0) 1      bc
        //       b\v    /
        //         B (6) -bd- D (12)
        //
        _onlyNodes: [
            0, 0, 0,
            1, 1, 6,
            1, 2, 12,
            1, 3, 18,
            1, 4, 21,
            1, 5, 21],
        _containmentEdges: [
            1,  6, 3, 1,  7, 6,
            0,  1, 6, 1,  8, 9,
            1,  9, 9, 1, 10, 12,
            1, 11, 15],
        _strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest.createHeapSnapshotMockRaw = function()
{
    return {
        snapshot: {},
        nodes: [
            { fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "children_count", "children"],
              types: [["hidden", "object"], "", "", "", "", "", "", { fields: ["type", "name_or_index", "to_node"], types: [["element", "property"], "", ""] }] },
            // Effectively the same graph as in createHeapSnapshotMockObject,
            // but having full set of fields.
            // 
            // A triple in parentheses indicates node index, self size and
            // retained size.
            // 
            //          --- A (14,2,2) --ac- C (40,4,10) -ce- E(57,6,6)
            //         /    |                /
            //  "" (1,0,20) 1       --bc-----
            //         \    v      /
            //          --- B (27,3,8) --bd- D (50,5,5)
            //
            0, 0, 1, 0, 20,  1, 2, 1,  6, 14, 1,  7, 27,
            1, 1, 2, 2,  2,  1, 2, 0,  1, 27, 1,  8, 40,
            1, 2, 3, 3,  8,  1, 2, 1,  9, 40, 1, 10, 50,
            1, 3, 4, 4, 10,  1, 1, 1, 11, 57,
            1, 4, 5, 5,  5, 27, 0,
            1, 5, 6, 6,  6, 40, 0],
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest.createHeapSnapshotSplitNodesEdgesMockRaw = function()
{
    // Return the same snapshot as above but having the nodes and edges
    // separated.
    return {
        snapshot: {},
        nodes: [
            {
                separate_edges: true,
                node_fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "edges_index"],
                node_types: [["hidden", "object"], "", "", "", "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "property"], "", ""]
            },
            0, 0, 1, 0, 20,  0,  0,
            1, 1, 2, 2,  2,  0,  6,
            1, 2, 3, 3,  8,  0, 12,
            1, 3, 4, 4, 10,  0, 18,
            1, 4, 5, 5,  5, 14, 21,
            1, 5, 6, 6,  6, 21, 21],
        edges: [
            1,  6,  7, 1,  7, 14,
            0,  1, 14, 1,  8, 21,
            1,  9, 21, 1, 10, 28,
            1, 11, 35],
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest._postprocessHeapSnapshotMock = function(mock)
{
    mock.metaNode = mock.nodes[0];
    mock.nodes[0] = 0;
    var tempNodes = new Int32Array(1000);
    tempNodes.set(mock.nodes);
    mock.nodes = tempNodes.subarray(0, mock.nodes.length);
    return mock;
};

InspectorTest.createHeapSnapshotMock = function()
{
    return InspectorTest._postprocessHeapSnapshotMock(InspectorTest.createHeapSnapshotMockRaw());
};

InspectorTest.createHeapSnapshotMockWithDOM = function()
{
    return InspectorTest._postprocessHeapSnapshotMock({
        snapshot: {},
        nodes: [
            { fields: ["type", "name", "id", "children_count", "children"],
              types: [["hidden", "object"], "", "", "", { fields: ["type", "name_or_index", "to_node"], types: [["element", "hidden", "internal"], "", ""] }] },
            // A tree with Window objects.
            //
            //    |----->Window--->A
            //    |                \
            //    |----->Window--->B--->C
            //    |        |     \
            //  (root)   hidden   --->D--internal / "native"-->N
            //    |         \         |
            //    |----->E   H     internal
            //    |                   v
            //    |----->F--->G------>M
            //
            /* (root) */    0,  0,  1, 4, 0,  1, 17, 0, 2, 27, 0, 3, 40, 0, 4, 44,
            /* Window */    1, 11,  2, 2, 0,  1, 51, 0, 2, 55,
            /* Window */    1, 11,  3, 3, 0,  1, 55, 0, 2, 62, 1, 3, 72,
            /* E */         1,  5,  4, 0,
            /* F */         1,  6,  5, 1, 0,  1, 76,
            /* A */         1,  1,  6, 0,
            /* B */         1,  2,  7, 1, 0,  1, 80,
            /* D */         1,  4,  8, 2, 2, 12, 84, 2, 1, 88,
            /* H */         1,  8,  9, 0,
            /* G */         1,  7, 10, 0,
            /* C */         1,  3, 11, 0,
            /* N */         1, 10, 12, 0,
            /* M */         1,  9, 13, 0
            ],
        strings: ["", "A", "B", "C", "D", "E", "F", "G", "H", "M", "N", "Window", "native"]
    });
};

};
