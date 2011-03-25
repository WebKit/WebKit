var initialize_HeapSnapshotTest = function() {

InspectorTest.createHeapSnapshotMockObject = function()
{
    return {
        _rootNodeIndex: 0,
        _nodeTypeOffset: 0,
        _nodeNameOffset: 1,
        _edgesCountOffset: 2,
        _firstEdgeOffset: 3,
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
        //         A (9) --ac- C (27) -ce- E(36)
        //       a/|         /
        //  "" (0) 1      bc
        //       b\v    /
        //         B (18) -bd- D (33)
        //
        _nodes: [
            0, 0, 2, 1,  6,  9, 1,  7, 18,
            1, 1, 2, 0,  1, 18, 1,  8, 27,
            1, 2, 2, 1,  9, 27, 1, 10, 33,
            1, 3, 1, 1, 11, 36,
            1, 4, 0,
            1, 5, 0],
        _strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest.createHeapSnapshotMock = function()
{
    return {
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

};
