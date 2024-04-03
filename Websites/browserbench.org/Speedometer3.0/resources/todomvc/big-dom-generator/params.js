/**
 * This file exports several constants that are used to generate a large DOM tree for the TodoMVC application
 * as well as to generate a CSS selector that targets a specific element in the generated DOM tree.
 */

// The default seed value for the random number generator.
export const DEFAULT_SEED_FOR_RANDOM_NUMBER_GENERATOR = 4212021;

// Constraints for the generated tree-view side panel for the complex DOM shell
export const MAX_GENERATED_DOM_DEPTH = 40;
export const MAX_NUMBER_OF_CHILDREN = 16;
export const PROBABILITY_OF_HAVING_CHILDREN = 0.3;
// The target element count for the generated DOM tree.
export const TARGET_SIZE = 6000;
// The generator will ensure that the generated DOM tree has
// at least MIN_NUMBER_OF_MAX_DEPTH_BRANCHES of depth MAX_GENERATED_DOM_DEPTH.
export const MIN_NUMBER_OF_MAX_DEPTH_BRANCHES = 2;
export const MAX_VISIBLE_TREE_VIEW_ITEM_DEPTH = 8;
export const PERCENTAGE_OF_DISPLAY_NONE_TREEVIEW_ELEMENTS = 0.5;
