import { LCG } from "random-seedable";
import { DEFAULT_SEED_FOR_RANDOM_NUMBER_GENERATOR, MAX_GENERATED_DOM_DEPTH, MAX_NUMBER_OF_CHILDREN, PROBABILITY_OF_HAVING_CHILDREN, TARGET_SIZE, MIN_NUMBER_OF_MAX_DEPTH_BRANCHES, PERCENTAGE_OF_DISPLAY_NONE_TREEVIEW_ELEMENTS } from "./../params";

const random = new LCG(DEFAULT_SEED_FOR_RANDOM_NUMBER_GENERATOR);

// Recursively depth-first computing subTreeWeight.
const fillSubtreeWeights = (node, expandableItemWeight, nonExpandableItemWeight) => {
    if (node.type === "expandableItem")
        node.subTreeWeight = node.children.reduce((acc, child) => acc + fillSubtreeWeights(child, expandableItemWeight, nonExpandableItemWeight), expandableItemWeight);
    else
        node.subTreeWeight = nonExpandableItemWeight;

    return node.subTreeWeight;
};

/*
 * Iterate over the exapandableItem nodes in a breadth-first manner until the
 * sum of weights of the subtrees with root nodes in the queue is less than
 * the target number of elements we want to have display none. Mark the
 * nodes in the queue as display none.
 * Consider the following example with the weights as displayed in the figure
 * and 10 as the target number of display none elements. The iteration will
 * stop with the nodes with weights 7 and 2 marked with *.
 *             20
 *          /      \
 *         12       8
 *        /  \     / \
 *       5   *7* *2*   6
 *           /   / \
 *          7   1   1
 */
const markDisplayNoneNodes = (node, expandableItemWeight, nonExpandableItemWeight) => {
    let currentSubTreesWeights = node.subTreeWeight;
    let currentIndex = 0;
    let nodeQueue = [node];
    while (currentSubTreesWeights >= TARGET_SIZE * PERCENTAGE_OF_DISPLAY_NONE_TREEVIEW_ELEMENTS) {
        const currentNode = nodeQueue[currentIndex];
        nodeQueue[currentIndex] = null;
        const expandableChildren = currentNode.children.filter((child) => child.type === "expandableItem");
        if (expandableChildren.length) {
            nodeQueue.push(...expandableChildren);
            currentSubTreesWeights -= expandableItemWeight;
            let numberOfNonExpandableChildren = currentNode.children.length - expandableChildren.length;
            currentSubTreesWeights -= numberOfNonExpandableChildren * nonExpandableItemWeight;
        } else {
            currentSubTreesWeights -= currentNode.subTreeWeight;
        }
        currentIndex++;
    }
    nodeQueue.forEach((node) => {
        if (node)
            node.isDisplayNone = true;
    });
};

/**
 * Generates the blueprint of the tree-view side panel for the complex DOM shell with expandable and non-expandable items.
 * It starts with the minimum number of maximum-depth branches and randomly adds
 * children to the nodes in a breadth-first manner.
 * The weight parameters represent how many DOM nodes are generated for each type of node.
 * @param {Object} options - The options object.
 * @param {number} options.expandableItemWeight - The weight for the "expandableItem" node type. <li></li> with ChevronRight svg.
 * @param {number} options.nonExpandableItemWeight - The weight for the "nonExpandableItem" node type. <li></li> TaskListIcon svg.
 * @returns {Object} The generated tree structure with two possible values for the "type" property: "expandableItem" or "nonExpandableItem"
 * Example structure:
 * {
 *    type: "expandableItem",
 *    children: [
 *      {
 *         type: "expandableItem",
 *         children: [
 *           {
 *             type: "nonExpandableItem",
 *             children: [],
 *             isDisplayNone: false,
 *             subTreeWeight: 0,
 *           }
 *         ],
 *         isDisplayNone: false,
 *        subTreeWeight: 0,
 *      }
 *    ],
 *    isDisplayNone: false,
 *    subTreeWeight: 0,
 * }
 **/
export const generateTreeHead = ({ expandableItemWeight, nonExpandableItemWeight }) => {
    const treeHead = { type: "expandableItem", children: [], isDisplayNone: false, subTreeWeight: 0 };
    const nodeWeight = { expandableItem: expandableItemWeight, nonExpandableItem: nonExpandableItemWeight };

    let totalNodes = expandableItemWeight;
    for (let i = 0; i < MIN_NUMBER_OF_MAX_DEPTH_BRANCHES; i++) {
        let currentDepth = 0;
        let currentNode = treeHead;
        while (currentDepth < MAX_GENERATED_DOM_DEPTH) {
            let childType = "expandableItem";
            currentNode.children.push({ type: childType, children: [] });
            currentNode = currentNode.children[currentNode.children.length - 1];
            currentDepth++;
            totalNodes += nodeWeight[childType];
        }
    }

    const treeNodes = [treeHead];
    // Do an outer loop because there is a chance that the inner loop ends without
    // reaching the target size.
    while (totalNodes < TARGET_SIZE) {
        let index = 0;
        // All items start as closed and are marked open if the algorithm adds children.
        while (index < treeNodes.length && totalNodes < TARGET_SIZE) {
            let currentNode = treeNodes[index];
            switch (currentNode.type) {
                case "expandableItem":
                    if (random.coin(PROBABILITY_OF_HAVING_CHILDREN) || currentNode.children.length) {
                        const numberOfNewChildren = random.randRange(1, MAX_NUMBER_OF_CHILDREN - currentNode.children.length + 1);
                        for (let i = 0; i < numberOfNewChildren && totalNodes < TARGET_SIZE; i++) {
                            currentNode.children.push({ type: "nonExpandableItem", children: [] });
                            totalNodes += nodeWeight["nonExpandableItem"];
                        }
                        random.shuffle(currentNode.children, true);
                        treeNodes.push(...currentNode.children);
                    }
                    break;
                case "nonExpandableItem":
                    if (random.coin(PROBABILITY_OF_HAVING_CHILDREN)) {
                        currentNode.type = "expandableItem";
                        // randomly choose the child type between expandableItem and nonExpandableItem
                        let childType = random.choice(["expandableItem", "nonExpandableItem"]);
                        currentNode.children.push({ type: childType, children: [] });
                        // We changed the node type so we need to update the totalNodes count.
                        totalNodes = totalNodes - nodeWeight["nonExpandableItem"] + nodeWeight["expandableItem"];
                        // We added a child so we need to update the totalNodes count.
                        totalNodes += nodeWeight[childType];
                        treeNodes.push(currentNode.children[0]);
                    }
                    break;
                default:
                    throw new Error(`Unknown node type: ${currentNode.type}`);
            }
            index++;
        }
    }

    fillSubtreeWeights(treeHead, expandableItemWeight, nonExpandableItemWeight);
    markDisplayNoneNodes(treeHead, expandableItemWeight, nonExpandableItemWeight);
    return treeHead;
};
