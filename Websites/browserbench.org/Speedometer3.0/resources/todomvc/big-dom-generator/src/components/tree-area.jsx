import { MAX_VISIBLE_TREE_VIEW_ITEM_DEPTH } from "../../params";
import { generateTreeHead } from "./../tree-generator";
import classNames from "classnames";

import ChevronRight from "./../assets/Smock_ChevronRight_18_N.svg";
import TaskListIcon from "./../assets/Smock_TaskList_18_N.svg";

const TreeItem = (props) => {
    const { treeNode, currentDepth } = props;

    const isExpandableItem = treeNode.type === "expandableItem";
    /**
     * Every expandable TreeItem is open by default unless it is at the MAX_VISIBLE_TREE_VIEW_ITEM_DEPTH threshold
     * or it is marked as display:none.
     **/
    const treeViewItemIsOpen = isExpandableItem && currentDepth !== MAX_VISIBLE_TREE_VIEW_ITEM_DEPTH && !treeNode.isDisplayNone;

    return (
        <li className={classNames("spectrum-TreeView-item", { "display-none": treeNode.isDisplayNone, "is-open": treeViewItemIsOpen }, `nodetype-${treeNode.type}`)}>
            {isExpandableItem
                ? <>
                    <a className="spectrum-TreeView-itemLink">
                        <ChevronRight className="spectrum-Icon spectrum-TreeView-itemIndicator spectrum-TreeView-itemIcon" />
                        <span className="just-span spectrum-TreeView-itemLabel">Sprint</span>
                    </a>
                    <ul className="spectrum-TreeView spectrum-TreeView--sizeS">
                        {treeNode.children.map((child) =>
                            <TreeItem treeNode={child} currentDepth={currentDepth + 1} />
                        )}
                    </ul>
                </>
                : <a className="spectrum-TreeView-itemLink">
                    <TaskListIcon className="task-list-icon spectrum-Icon spectrum-TreeView-itemIndicator spectrum-TreeView-itemIcon spectrum-Icon--sizeM" />
                    <span className="just-span spectrum-TreeView-itemLabel">Todo List</span>
                </a>
            }
        </li>
    );
};

/**
 * TreeItem generates either 6 or 10 elements because ChevronRight is
 * inlined as a svg element with one path child and TaskListIcon is
 * inlined as a svg element with 4 path children and one defs child.
 *
 * To reach 6 elements, the following dom structure is generated (expandableItem, chevronRight):
 * <li class="spectrum-TreeView-item is-open">
 *  <a class="spectrum-TreeView-itemLink">
 *      <svg>
 *          <path></path>
 *      </svg>
 *      <span>Sprint</span>
 *  </a>
 *  <ul></ul>
 * </li>
 * To reach 10 elements, the following dom structure is generated (nonExpandableItem, TaskListIcon):
 * <li class="spectrum-TreeView-item">
 *   <a class="spectrum-TreeView-itemLink">
 *     <svg>
 *       <defs>
 *         <style></style>
 *       </defs>
 *       <path></path>
 *       <path></path>
 *       <rect></rect>
 *       <rect></rect>
 *     </svg>
 *     <span>Todo List</span>
 *   </a>
 * </li>
 */
export const TreeArea = () => {
    const treeOptions = { expandableItemWeight: 6, nonExpandableItemWeight: 10 };
    const treeHead = generateTreeHead(treeOptions);
    return (
        <div className="tree-area">
            <h4 className="spectrum-Heading spectrum-Heading--sizeXS">Sprints</h4>
            <ul className="spectrum-TreeView spectrum-TreeView--sizeS">
                <TreeItem treeNode={treeHead} currentDepth={0} />
            </ul>
        </div>
    );
};
