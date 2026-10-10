/*
 * Progressive enhancement for the tests page's filter/sort chip surface.
 *
 * Everything here is optional: the surface is exploded per-chip <select>/<input> controls with
 * ordinary names, so a browser with this file blocked can still add, edit and remove a chip and
 * submit the form. This file only removes the round trip: it swaps a chip's operator and value
 * controls to match a newly chosen column and adds a chip without a request. Editing a chip never
 * submits on its own, since a chip is three controls and the reader is not done after the first.
 *
 * The column/operator/vocabulary data comes from the <script type="application/json"> block the
 * template renders from the same `filters` registry the server validates against, so nothing here
 * duplicates a vocabulary the server could drift away from.
 */

/**
 * Reads the page's registry JSON block, or null where it is missing or unparsable — a stale or
 * hand-edited page must not throw finding it.
 */
function readRegistry() {
    var block = document.getElementById('filter-registry');
    if (!block) {
        return null;
    }
    try {
        return JSON.parse(block.textContent);
    } catch (error) {
        return null;
    }
}

/**
 * Builds one option element, selected when its value matches `selected`.
 */
function buildOption(value, label, selected) {
    var option = document.createElement('option');
    option.value = value;
    option.textContent = label;
    if (selected) {
        option.selected = true;
    }
    return option;
}

/**
 * Replaces a chip's operator <select> with the options a newly chosen column allows, keeping the
 * previous operator selected where the new column still offers it.
 */
function rebuildOperatorControl(operatorSelect, operators, previousOperator) {
    operatorSelect.innerHTML = '';
    operatorSelect.appendChild(buildOption('', '', false));
    operators.forEach(function (operator) {
        operatorSelect.appendChild(
            buildOption(operator.name, operator.label, operator.name === previousOperator));
    });
}

/**
 * The value(s) a chip's current value control holds: every selected option of a multiple select, the
 * one selected option of a single select, or an input's own value — always as an array, so the one
 * caller that rebuilds the control never has to branch on which kind it started as.
 */
function controlValues(control) {
    if (control.multiple) {
        return Array.prototype.slice.call(control.selectedOptions).map(function (option) {
            return option.value;
        });
    }
    return control.value ? [control.value] : [];
}

/**
 * The arity (`registry`'s `NO_VALUES`/`ONE_VALUE`/`MANY_VALUES` spelling) of one named operator among
 * `operators`, or null where that operator is not in the list at all — a chip with no operator chosen
 * yet, for instance.
 */
function operatorArity(operators, operatorName) {
    var found = (operators || []).filter(function (operator) {
        return operator.name === operatorName;
    })[0];
    return found ? found.arity : null;
}

/**
 * Replaces a chip's value control with a vocabulary <select> or a typed <input>, matching what the
 * server would have rendered for the same column and the same operator: a many-value operator on a
 * vocabulary column gets a `<select multiple>` with every previous value preselected, a one-value
 * operator on the same column gets the single select it always got, and a column with no vocabulary
 * still gets a typed input regardless of arity — free text already carries a many-value operator's
 * `LIST_SEPARATOR`-joined values without a control of its own.
 */
function rebuildValueControl(valueControl, column, name, manyValues, previousValues) {
    var replacement;
    if (column.vocabulary && manyValues) {
        replacement = document.createElement('select');
        replacement.multiple = true;
        column.vocabulary.forEach(function (choice) {
            replacement.appendChild(
                buildOption(choice, choice, previousValues.indexOf(choice) !== -1));
        });
    } else if (column.vocabulary) {
        replacement = document.createElement('select');
        replacement.appendChild(buildOption('', '', false));
        column.vocabulary.forEach(function (choice) {
            replacement.appendChild(buildOption(choice, choice, choice === (previousValues[0] || '')));
        });
    } else {
        replacement = document.createElement('input');
        replacement.type = column.inputType || 'text';
        replacement.value = previousValues[0] || '';
    }
    replacement.name = name;
    replacement.className = valueControl.className;
    replacement.setAttribute('aria-label', valueControl.getAttribute('aria-label') || '');
    valueControl.replaceWith(replacement);
    return replacement;
}

/**
 * The index a chip's controls were named with, read off its column select's own name
 * (`f.tests:<index>:column` or `s.tests:<index>:column`), or null where the name does not match.
 */
function chipIndex(columnSelect) {
    var match = /:(\d+):column$/.exec(columnSelect.name || '');
    return match ? match[1] : null;
}

/**
 * Reacts to a filter chip's column changing: rebuilds that chip's operator and value controls for
 * the newly chosen column, using the "any operator" list where the column was cleared back to
 * "No filter" — the same fallback the server renders for a chip with no column yet.
 */
function onFilterColumnChanged(registry, columnSelect) {
    var chip = columnSelect.closest('.chip');
    if (!chip) {
        return;
    }
    var operatorSelect = chip.querySelector('.chip-operator');
    var valueControl = chip.querySelector('.chip-value');
    if (!operatorSelect || !valueControl) {
        return;
    }
    var index = chipIndex(columnSelect);
    var column = registry.columns[columnSelect.value];
    var operators = column ? column.operators : registry.anyOperators;
    var previousOperator = operatorSelect.value;
    rebuildOperatorControl(operatorSelect, operators || [], previousOperator);
    var valueName = index === null ? valueControl.name
        : registry.filterArgument + ':' + index + ':value';
    var manyValues = operatorArity(operators, operatorSelect.value) === 'many';
    rebuildValueControl(valueControl, column || {inputType: 'text'}, valueName, manyValues,
        controlValues(valueControl));
}

/**
 * Reacts to a filter chip's operator changing: a vocabulary column's value control has to switch
 * between a single and a multiple select as the reader moves between (for instance) `is`/`is any of`
 * on the same column, without touching the column select at all.
 */
function onFilterOperatorChanged(registry, operatorSelect) {
    var chip = operatorSelect.closest('.chip');
    if (!chip) {
        return;
    }
    var columnSelect = chip.querySelector('.chip-column');
    var valueControl = chip.querySelector('.chip-value');
    if (!columnSelect || !valueControl) {
        return;
    }
    var index = chipIndex(columnSelect);
    var column = registry.columns[columnSelect.value];
    var operators = column ? column.operators : registry.anyOperators;
    var valueName = index === null ? valueControl.name
        : registry.filterArgument + ':' + index + ':value';
    var manyValues = operatorArity(operators, operatorSelect.value) === 'many';
    rebuildValueControl(valueControl, column || {inputType: 'text'}, valueName, manyValues,
        controlValues(valueControl));
}

/**
 * Appends one more blank chip to a row, named for the next index in that row, instead of a round
 * trip through the "+ filter"/"+ sort" button's own submission.
 */
function addChip(row, registry, kind) {
    var existing = row.querySelectorAll('.chip').length;
    var chip = document.createElement('span');
    chip.className = 'chip';
    var argument = kind === 'filter' ? registry.filterArgument : registry.sortArgument;
    var columnSelect = document.createElement('select');
    columnSelect.className = 'chip-column';
    columnSelect.name = argument + ':' + existing + ':column';
    columnSelect.appendChild(buildOption('', kind === 'filter' ? 'No filter' : 'No sort', false));
    Object.keys(registry.columns).forEach(function (name) {
        var column = registry.columns[name];
        if (kind === 'filter' ? column.filterable : column.sortable) {
            columnSelect.appendChild(buildOption(name, column.label, false));
        }
    });
    chip.appendChild(columnSelect);
    if (kind === 'filter') {
        var operatorSelect = document.createElement('select');
        operatorSelect.className = 'chip-operator';
        operatorSelect.name = argument + ':' + existing + ':op';
        operatorSelect.appendChild(buildOption('', '', false));
        registry.anyOperators.forEach(function (operator) {
            operatorSelect.appendChild(buildOption(operator.name, operator.label, false));
        });
        chip.appendChild(operatorSelect);
        var valueInput = document.createElement('input');
        valueInput.type = 'text';
        valueInput.className = 'chip-value';
        valueInput.name = argument + ':' + existing + ':value';
        chip.appendChild(valueInput);
    } else {
        var directionSelect = document.createElement('select');
        directionSelect.className = 'chip-direction';
        directionSelect.name = argument + ':' + existing + ':direction';
        directionSelect.appendChild(buildOption('asc', 'Ascending', true));
        directionSelect.appendChild(buildOption('desc', 'Descending', false));
        chip.appendChild(directionSelect);
    }
    row.appendChild(chip);
}

/**
 * Wires one chip form: column changes rebuild that chip's operator/value controls, and the add
 * buttons grow a row client-side instead of round-tripping. A control change deliberately does not
 * submit: a chip is edited across three controls, so submitting on the first of them would reload
 * the page before the reader has said what to compare against.
 */
function wireChipForm(form) {
    var registry = readRegistry();
    if (!registry) {
        return;
    }
    form.addEventListener('change', function (event) {
        var target = event.target;
        if (!target || !(target instanceof Element)) {
            return;
        }
        if (target.classList.contains('chip-column') && target.closest('[data-chip-kind="filter"]')) {
            onFilterColumnChanged(registry, target);
        }
        if (target.classList.contains('chip-operator') && target.closest('[data-chip-kind="filter"]')) {
            onFilterOperatorChanged(registry, target);
        }
    });
    form.addEventListener('click', function (event) {
        var target = event.target;
        if (!target || !(target instanceof Element)) {
            return;
        }
        var kind = null;
        var addFilterButton = target.closest('button[name="add_filter"]');
        var addSortButton = target.closest('button[name="add_sort"]');
        var row = null;
        if (addFilterButton) {
            row = form.querySelector('[data-chip-kind="filter"]');
            kind = 'filter';
        } else if (addSortButton) {
            row = form.querySelector('[data-chip-kind="sort"]');
            kind = 'sort';
        }
        if (row && kind) {
            event.preventDefault();
            addChip(row, registry, kind);
        }
    });
}

/*
 * Progressive enhancement for the queue picker's checkbox tree.
 *
 * The server unions whatever `group`/`version`/`builder` parameters a submitted GET request carries,
 * and a ticked box submits its own name with no help from here: with this file blocked, ticking a
 * group alone still submits `group=<name>` and every reader-visible control keeps working. What this
 * file adds is two things a static tree cannot do on its own: ticking a parent visually ticks its
 * children (and vice versa on unticking, up the chain), and it strips the children's own inputs back
 * out of the submission when a parent is ticked, so the request that leaves the browser stays exactly
 * `group=macOS` rather than every builder macOS happened to contain today. That distinction matters
 * because `group=macOS` also covers a builder added to the group next month; a URL that enumerated
 * today's builders would not.
 */

/**
 * The checkbox inputs nested under `input`'s own subtree — every version and builder a ticked group
 * contains, or every builder a ticked version contains — or an empty list for a builder leaf, which
 * has no `<details>` of its own to hold any.
 */
function queueTreeDescendants(input) {
    var branch = input.nextElementSibling;
    if (!branch || !branch.matches('details.tree-node')) {
        return [];
    }
    return branch.querySelectorAll('input[type=checkbox]');
}

/**
 * `input`'s own containing parent checkboxes, nearest first — the version above a builder and the
 * group above that, or just the group above a version, or nothing above a group.
 */
function queueTreeAncestors(input) {
    var ancestors = [];
    var node = input.closest('li.node');
    var container = node ? node.parentElement : null;
    var ancestorNode = container ? container.closest('li.node') : null;
    while (ancestorNode) {
        var ancestorInput = ancestorNode.querySelector('input[type=checkbox]');
        if (ancestorInput) {
            ancestors.push(ancestorInput);
        }
        container = ancestorNode.parentElement;
        ancestorNode = container ? container.closest('li.node') : null;
    }
    return ancestors;
}

/**
 * The row a checkbox's own "checked because a parent is" marker belongs on: the label wrapping a
 * builder leaf's, or the flex row beside a group or version checkbox.
 */
function queueTreeRow(input) {
    return input.closest('label') || input.closest('.row');
}

function setQueueTreeImplied(input, implied) {
    var row = queueTreeRow(input);
    if (row) {
        row.classList.toggle('implied', implied);
    }
}

/**
 * A parent whose descendants are only partly ticked reads as indeterminate rather than ticked or
 * blank, on load from the server-rendered state as much as after any later change — arriving at a
 * link that ticked one builder under a group must show that group and its version as indeterminate,
 * not blank.
 */
function updateQueueTreeIndeterminate(form) {
    form.querySelectorAll('input[name="group"], input[name="version"]').forEach(function (parent) {
        var descendants = queueTreeDescendants(parent);
        if (!descendants.length) {
            return;
        }
        var checked = 0;
        descendants.forEach(function (descendant) {
            if (descendant.checked) {
                checked += 1;
            }
        });
        parent.indeterminate = checked > 0 && checked < descendants.length;
    });
}

/**
 * Ticking a parent ticks every descendant and marks each one implied rather than explicit; unticking
 * it clears both. Unticking any single box, parent or leaf, also unticks every ancestor above it —
 * without touching that ancestor's other descendants' own checked state, so a reader who unticks one
 * builder under a ticked group is left with the other builders still individually ticked rather than
 * the whole group vanishing.
 */
function applyQueueTreeChange(target) {
    var descendants = queueTreeDescendants(target);
    if (target.checked) {
        descendants.forEach(function (descendant) {
            descendant.checked = true;
            setQueueTreeImplied(descendant, true);
        });
        setQueueTreeImplied(target, false);
        return;
    }
    descendants.forEach(function (descendant) {
        descendant.checked = false;
        setQueueTreeImplied(descendant, false);
    });
    setQueueTreeImplied(target, false);
    queueTreeAncestors(target).forEach(function (ancestor) {
        if (!ancestor.checked) {
            return;
        }
        ancestor.checked = false;
        setQueueTreeImplied(ancestor, false);
        queueTreeDescendants(ancestor).forEach(function (sibling) {
            setQueueTreeImplied(sibling, false);
        });
    });
}

/**
 * At submit time only, a ticked group or version disables the descendants its own value already
 * covers, so a disabled (and therefore unsubmitted) input is the only difference between what this
 * reader ticked and what leaves as the query string. Descendants stay enabled up to that moment,
 * since a disabled checkbox cannot be unticked and rule 3 above depends on being able to untick one.
 */
function collapseQueueTreeSubmission(form) {
    form.querySelectorAll('input[name="group"]:checked').forEach(function (group) {
        queueTreeDescendants(group).forEach(function (descendant) {
            descendant.disabled = true;
        });
    });
    form.querySelectorAll('input[name="version"]:checked').forEach(function (version) {
        queueTreeDescendants(version).forEach(function (descendant) {
            descendant.disabled = true;
        });
    });
}

function wireQueueTree(picker) {
    var form = picker.querySelector('form.filter-form');
    if (!form) {
        return;
    }
    form.addEventListener('change', function (event) {
        var target = event.target;
        if (!target || !(target instanceof Element) || target.type !== 'checkbox') {
            return;
        }
        if (['group', 'version', 'builder'].indexOf(target.name) === -1) {
            return;
        }
        applyQueueTreeChange(target);
        updateQueueTreeIndeterminate(form);
    });
    form.addEventListener('submit', function () {
        collapseQueueTreeSubmission(form);
    });
    updateQueueTreeIndeterminate(form);
}

document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('form.chip-form').forEach(wireChipForm);
    document.querySelectorAll('.queue-tree').forEach(wireQueueTree);
});
