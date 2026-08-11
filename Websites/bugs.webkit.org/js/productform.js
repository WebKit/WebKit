/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 */

// Functions to update form select elements based on a
// collection of javascript arrays containing strings.

/**
 * Reads the selected products and updates the component list accordingly.
 *
 * @param  product    Select element that contains products.
 * @param  component  Select element that contains components.
 * @param  anyval     Value to use for a special "Any" list item. Can be null
 *                    to not use any. If used must and will be first item in
 *                    the select element.
 *
 * @global cpts       Array of arrays, indexed by product name. The subarrays
 *                    contain a list of components to be fed to the respective
 *                    select element.
 * @global first_load Boolean; true if this is the first time this page loads
 *                    or false if not.
 * @global last_sel   Array that contains last list of products so we know what
 *                    has changed, and optimize for additions.
 */
function selectProduct(product, component, anyval) {
    // This is to avoid handling events that occur before the form
    // itself is ready, which could happen in buggy browsers.
    if (!product || !component)
        return;

    // Do nothing if no products are defined. This is to avoid the
    // "a has no properties" error from merge_arrays function.
    if (product.length == (anyval != null ? 1 : 0))
        return;

    // If this is the first load and nothing is selected, no need to
    // merge and sort all lists; they are created sorted.
    if ((first_load) && (product.selectedIndex == -1)) {
        first_load = false;
        return;
    }

    // Turn first_load off. This is tricky, since it seems to be
    // redundant with the above clause. It's not: if when we first load
    // the page there is _one_ element selected, it won't fall into that
    // clause, and first_load will remain 1. Then, if we unselect that
    // item, selectProduct will be called but the clause will be valid
    // (since selectedIndex == -1), and we will return - incorrectly -
    // without merge/sorting.
    first_load = false;

    // Stores products that are selected.
    var sel = Array();

    // True if sel array has a full list or false if sel contains only
    // new products that are to be merged to the current list.
    var merging = false;

    // If nothing is selected, or the special "Any" option is selected
    // which represents all products, then pick all products so we show
    // all components.
    var findall = (product.selectedIndex == -1
                   || (anyval != null && product.options[0].selected));

    sel = get_selection(product, findall, false, anyval);

    if (!findall) {
        // Save sel for the next invocation of selectProduct().
        var tmp = sel;

        // This is an optimization: if we have just added products to an
        // existing selection, no need to clear the form controls and add
        // everybody again; just merge the new ones with the existing
        // options.
        if ((last_sel.length > 0) && (last_sel.length < sel.length)) {
            sel = fake_diff_array(sel, last_sel);
            merging = true;
        }
        last_sel = tmp;
    }

    // Do the actual fill/update.
    var saved_cpts = get_selection(component, false, true, null);
    updateSelect(cpts, sel, component, merging, anyval);
    restoreSelection(component, saved_cpts);
}

/**
 * Adds to the target select element all elements from array that
 * correspond to the selected items.
 *
 * @param array   An array of arrays, indexed by number. The array should
 *                contain elements for each selection.
 * @param sel     A list of selected items, either whole or a diff depending
 *                on merging parameter.
 * @param target  Select element that is to be updated.
 * @param merging Boolean that determines if we are merging in a diff or
 *                substituting the whole selection. A diff is used to optimize
 *                adding selections.
 * @param anyval  Name of special "Any" value to add. Can be null if not used.
 * @return        Boolean; true if target contains options or false if target
 *                is empty.
 *
 * Example (compsel is a select form element):
 *
 *     var components = Array();
 *     components[1] = [ 'ComponentA', 'ComponentB' ];
 *     components[2] = [ 'ComponentC', 'ComponentD' ];
 *     source = [ 2 ];
 *     updateSelect(components, source, compsel, false, null);
 *
 * This would clear compsel and add 'ComponentC' and 'ComponentD' to it.
 */
function updateSelect(array, sel, target, merging, anyval) {
    var i, item;

    // If we have no versions/components/milestones.
    if (array.length < 1) {
        target.options.length = 0;
        return false;
    }

    if (merging) {
        // Array merging/sorting in the case of multiple selections
        // merge in the current options with the first selection.
        item = merge_arrays(array[sel[0]], target.options, 1);

        // Merge the rest of the selection with the results.
        for (i = 1 ; i < sel.length ; i++)
            item = merge_arrays(array[sel[i]], item, 0);
    }
    else if (sel.length > 1) {
        // Here we micro-optimize for two arrays to avoid merging with a
        // null array.
        item = merge_arrays(array[sel[0]],array[sel[1]], 0);

        // Merge the arrays. Not very good for multiple selections.
        for (i = 2; i < sel.length; i++)
            item = merge_arrays(item, array[sel[i]], 0);
    }
    else {
        // Single item in selection, just get me the list.
        item = array[sel[0]];
    }

    // Clear current selection.
    target.options.length = 0;

    // Add special "Any" value back to the list.
    if (anyval != null)
        target.options[0] = new Option(anyval, "");

    // Load elements of list into select element.
    for (i = 0; i < item.length; i++)
        target.options[target.options.length] = new Option(item[i], item[i]);

    return true;
}

/**
 * Selects items in select element that are defined to be selected.
 *
 * @param control  Select element of which selected options are to be restored.
 * @param selnames Array of option names to select.
 */
function restoreSelection(control, selnames) {
    // Right. This sucks but I see no way to avoid going through the
    // list and comparing to the contents of the control.
    for (var j = 0; j < selnames.length; j++)
        for (var i = 0; i < control.options.length; i++)
            if (control.options[i].value == selnames[j])
                control.options[i].selected = true;
}

/**
 * Returns elements in a that are not in b.
 * NOT A REAL DIFF: does not check the reverse.
 *
 * @param  a First array to compare.
 * @param  b Second array to compare.
 * @return   Array of elements in a but not in b.
 */
function fake_diff_array(a, b) {
    var newsel = new Array();
    var found = false;

    // Do a boring array diff to see who's new.
    for (var ia in a) {
        for (var ib in b)
            if (a[ia] == b[ib])
                found = true;

        if (!found)
            newsel[newsel.length] = a[ia];

        found = false;
    }

    return newsel;
}

/**
 * Takes two arrays and sorts them by string, returning a new, sorted
 * array. The merge removes dupes, too.
 *
 * @param  a           First array to merge.
 * @param  b           Second array or an optionitem element to merge.
 * @param  b_is_select Boolean; true if b is an optionitem element (need to
 *                     access its value by item.value) or false if b is a
 *                     an array.
 * @return             Merged and sorted array.
 */
function merge_arrays(a, b, b_is_select) {
    var pos_a = 0;
    var pos_b = 0;
    var ret = new Array();
    var bitem, aitem;

    // Iterate through both arrays and add the larger item to the return
    // list. Remove dupes, too. Use toLowerCase to provide
    // case-insensitivity.
    while ((pos_a < a.length) && (pos_b < b.length)) {
        aitem = a[pos_a];
        if (b_is_select)
            bitem = b[pos_b].value;
        else
            bitem = b[pos_b];

        // Smaller item in list a.
        if (aitem.toLowerCase() < bitem.toLowerCase()) {
            ret[ret.length] = aitem;
            pos_a++;
        }
        else {
            // Smaller item in list b.
            if (aitem.toLowerCase() > bitem.toLowerCase()) {
                ret[ret.length] = bitem;
                pos_b++;
            }
            else {
                // List contents are equal, include both counters.
                ret[ret.length] = aitem;
                pos_a++;
                pos_b++;
            }
        }
    }

    // Catch leftovers here. These sections are ugly code-copying.
    if (pos_a < a.length)
        for (; pos_a < a.length ; pos_a++)
            ret[ret.length] = a[pos_a];

    if (pos_b < b.length) {
        for (; pos_b < b.length; pos_b++) {
            if (b_is_select)
                bitem = b[pos_b].value;
            else
                bitem = b[pos_b];
            ret[ret.length] = bitem;
        }
    }

    return ret;
}

/**
 * Returns an array of indexes or values of options in a select form element.
 *
 * @param  control     Select form element from which to find selections.
 * @param  findall     Boolean; true to return all options or false to return
 *                     only selected options.
 * @param  want_values Boolean; true to return values and false to return
 *                     indexes.
 * @param  anyval      Name of a special "Any" value that should be skipped. Can
 *                     be null if not used.
 * @return             Array of all or selected indexes or values.
 */
function get_selection(control, findall, want_values, anyval) {
    var ret = new Array();

    if ((!findall) && (control.selectedIndex == -1))
        return ret;

    for (var i = (anyval != null ? 1 : 0); i < control.length; i++)
        if (findall || control.options[i].selected)
            ret[ret.length] = want_values ? control.options[i].value : i;

    return ret;
}
