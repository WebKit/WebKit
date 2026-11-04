// Copyright (C) 2019-2021 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import {DOM, REF} from '/library/js/Ref.js';
import {deepCompare, queryToParams, paramsToQuery, QueryModifier, escapeEndpoint, escapeHTML} from '/assets/js/common.js';
import {CommitBank} from '/assets/js/commit.js';
import {Configuration} from '/assets/js/configuration.js'

function setEnableRecursive(element, state) {
    element.disabled = !state;
    if (!state)
        element.classList.add("disabled");
    else
        element.classList.remove("disabled");

    for (let node of element.children)
        setEnableRecursive(node, state);
}

function Drawer(controls = [], onCollapseChange) {
    const HIDDEN = false;
    const VISIBLE = true;
    let drawerState = VISIBLE;
    let mains = [];

    const sidebarControl = document.getElementsByClassName('mobile-sidebar-control')[0];
    sidebarControl.classList.add('display');

    const drawerRef = REF.createRef({
        state: drawerState,
        onStateUpdate: (element, state) => {
            if (state) {
                element.classList.remove("hidden");
                mains.forEach(main => main.classList.remove("hidden"));
            } else {
                element.classList.add("hidden");
                mains.forEach(main => main.classList.add("hidden"));
            }

            for (let node of element.children) {
                if (node.classList.contains("list"))
                    setEnableRecursive(node, state);
            }
            
            if (onCollapseChange)
                onCollapseChange();
        },
        onElementMount: (element) => {
            let candidates = document.getElementsByClassName("main");
            mains = [];
            for (let count = 0; count < candidates.length; ++count)
                mains.push(candidates[count]);

            sidebarControl.onclick = () => {
                if (element.style.display)
                    element.style.display = null;
                else {
                    for (let node of element.children) {
                        if (node.classList.contains("list"))
                            setEnableRecursive(node, true);
                    }
                    element.style.display = 'block';
                }
            }
        }
    });

    const drawerControllerRef = REF.createRef({
        state: drawerState,
        onStateUpdate: (element, state) => {
            if (state) {
                element.innerHTML = 'Collapse &gt';
                element.style.textAlign = 'center';
            }
            else{
                element.innerHTML = '&lt';
                element.style.textAlign = 'left';
            }
        },
        onElementMount: (element) => {
            element.onclick = () => {
                drawerState = !drawerState;
                drawerRef.setState(drawerState);
                drawerControllerRef.setState(drawerState);
            }
        }
    });

    return `<div class="sidebar right under-topbar-with-actions unselectable" ref="${drawerRef}">
            <button class="button desktop-control" ref="${drawerControllerRef}" style="cursor: pointer; width:96%; margin: 10px 2% 10px 2%;"></button>
            ${controls.map(control => {
                return `<div class="list">
                        <div class="item">${control}</div>
                    </div>`;
                }).join('')}
        </div>`;
}

const SEARCH_HOT_KEY = '/';

function isTextEntryFocused() {
    const element = document.activeElement;
    if (!element)
        return false;
    if (element.isContentEditable)
        return true;
    switch (element.tagName) {
        case 'TEXTAREA':
        case 'SELECT':
            return true;
        case 'INPUT':
            return !['checkbox', 'range'].includes(element.type);
        default:
            return false;
    }
}

function CommitSearchBar(onSearchAction = null) {
    const searchInputRef = REF.createRef({
        onElementMount: () => {
            window.addEventListener("keydown", searchHotKeyFunction);
        },
        onElementUnmount: () => {
            window.removeEventListener("keydown", searchHotKeyFunction);
        }
    });
    const searchHotKeyFunction = (e) => {
        if (e.key === "Escape") {
            const element = searchInputRef.element;
            if (!element || document.activeElement !== element)
                return;

            e.preventDefault();
            element.blur();
            return;
        }

        // Return early unless it's '/' without Ctrl, Cmd or Alt, so browser shortcuts still work.
        if (e.key !== SEARCH_HOT_KEY || e.ctrlKey || e.metaKey || e.altKey)
            return;
        if (isTextEntryFocused())
            return;

        const element = searchInputRef.element;
        if (element.disabled)
            return;

        e.preventDefault();
        element.focus();
    };
    const searchInputEventStream = searchInputRef.fromEvent("keyup");
    searchInputEventStream.action((e) => {
        const searchValue = searchInputRef.element.value;
        if (e.key === "Enter") {
            if (onSearchAction)
                onSearchAction(searchValue);
        }
    });
    
    const searchButtonRef = REF.createRef({});
    const searchButtonClickEventStream = searchButtonRef.fromEvent("click");
    searchButtonClickEventStream.action((e) => {
        const searchValue = searchInputRef.element.value;
        if (onSearchAction)
            onSearchAction(searchValue);
    });

    const clearButtonRef = REF.createRef({});
    clearButtonRef.fromEvent("click").action(e => {
        if (onSearchAction)
            onSearchAction(null);
    });

    return `<div class="input">
        <div class="row">
            <div class="input col-7">
                <input type="text" ref="${searchInputRef}" autocomplete="off" autocapitalize="none" required/>
                <label>Find commit (${SEARCH_HOT_KEY})</label>
            </div>
            <button class="button col-3 primary" ref="${searchButtonRef}">
                <img src="library/icons/search.svg" style="height: var(--largeSize); filter: invert(1);">
            </button>
            <button class="button col-2" ref="${clearButtonRef}">
                X
            </button>
        </div>
    </div>`;
}

let configurations = []
let configurationsDefinedCallbacks = [];
function refreshConfigurations() {
    let params = queryToParams(document.URL.split('?')[1]);
    let queryParams = {};
    if (params.branch)
        queryParams.branch = params.branch;

    const query = paramsToQuery(queryParams);
    fetch(query ? 'api/suites?' + query: 'api/suites').then(response => {
        response.json().then(json => {
            configurations = json.map(pair => new Configuration(pair[0]));
            configurationsDefinedCallbacks.forEach(callback => callback());
        });
    }).catch(error => {
        // If the load fails, log the error and continue
        console.error(JSON.stringify(error, null, 4));
    });
}

function BranchSelector(callback) {
    const defaultBranches = new Set(['trunk', 'master', 'main']);
    const defaultBranchKey = [...defaultBranches].sort().join('/');
    const branchModifier = new QueryModifier('branch');

    let ref = REF.createRef({
        state: [defaultBranchKey],
        onElementMount: (element) => {
            element.onchange = () => {
                let branch = element.value;
                if (branch === defaultBranchKey)
                    branchModifier.remove();
                else
                    branchModifier.replace(branch);
                refreshConfigurations();
                callback();
            }
        },
        onStateUpdate: (element, state) => {
            const branchQuery = branchModifier.current().length ? branchModifier.current()[branchModifier.current().length -1]:null;
            element.innerHTML = state.map(branch => {
                if (!branch)
                    return '';
                if (branch === branchQuery)
                    return `<option selected value="${branch}">${branch}</option>`;
                return `<option value="${branch}">${branch}</option>`;
            }).join('');
            element.parentElement.parentElement.parentElement.style.display = state.length > 1 ? null : 'none';
        },
    });

    fetch('api/commits/branches').then(response => {
        response.json().then(json => {
            let branchNames = new Set();
            if (branchModifier.current().length)
                branchNames.add(branchModifier.current()[branchModifier.current().length -1]);
            Object.keys(json).forEach(repo => {
                json[repo].forEach(branch => {
                    if (!defaultBranches.has(branch))
                        branchNames.add(branch);
                });
            });
            branchNames = [...branchNames];
            branchNames.sort();
            branchNames.splice(0, 0, defaultBranchKey);
            ref.setState(branchNames);
        });
    }).catch(error => {
        // If the load fails, log the error and continue
        console.error(JSON.stringify(error, null, 4));
    });

    return `<div class="input">
            <select required ref="${ref}"></select>
            <label>Branch</label>
        </div>`;
}

const SUB_SUITE_RELOAD_DELAY = 400;
const SUB_SUITE_FILTER_ROW_LIMIT = 200;
const SUB_SUITE_ROW_SIZE = '13px';
const SUB_SUITE_ROW_STYLE = 'margin: 0; padding-top: 2px; padding-bottom: 2px; padding-right: calc(var(--smallSize) * 2 + 8px)';
const SUB_SUITE_INDENT = 12;
const SUB_SUITE_LIST_HEIGHT = '300px';
const SUB_SUITE_BOX_INSET = '5px';
const SUB_SUITE_NOTE_STYLE = 'flex: none; margin-left: 0.5em; font-style: italic';
const SUB_SUITE_LINE = 'linear-gradient(var(--greyDarker), var(--greyDarker))';

function subSuitesInAddress(suites) {
    const picked = new Map();
    new QueryModifier('sub_suite').current().forEach(value => {
        const suite = suites.filter(name => value.startsWith(`${name}/`) && value.length > name.length + 1).reduce((longest, name) => (longest && longest.length >= name.length ? longest : name), null);
        if (!suite)
            return;
        const folders = picked.get(suite) || [];
        const folder = value.slice(suite.length + 1);
        if (!folders.includes(folder))
            folders.push(folder);
        picked.set(suite, folders);
    });
    return picked;
}

function suitesOnShow(suites) {
    const onShow = new QueryModifier('suite').current();
    return onShow.length ? suites.filter(suite => onShow.includes(suite)) : suites;
}

function writeSuitesOnShow(suites, onShow, {clearSubSuites = false} = {}) {
    const splitURL = document.URL.split('?');
    const params = queryToParams(splitURL[1]);
    if (onShow.length === suites.length)
        delete params.suite;
    else
        params.suite = onShow.length ? onShow : [''];
    if (clearSubSuites)
        delete params.sub_suite;
    const queryString = paramsToQuery(params);
    window.history.pushState(queryString, '', splitURL[0] + (queryString ? '?' + queryString : ''));
}

function SubSuiteSelector(suites, onSuitesChange, callback) {
    const modifier = new QueryModifier('sub_suite');
    const picked = subSuitesInAddress(suites);
    let listed = suites;
    const sections = [];
    const sectionFor = suite => {
        let section = sections.find(candidate => candidate.suite === suite);
        if (!section) {
            section = {
                suite,
                list: null,
                nodes: new Map(),
                topLevel: [],
                openGroups: new Set(),
                found: new Set(),
                foundAndAbove: new Set(),
                openWhileFiltering: new Set(),
                isExpanded: picked.has(suite),
                isExpandedWhileFiltering: true,
                isRequested: false,
                message: '',
            };
            sections.push(section);
        }
        return section;
    };
    const listedSections = () => listed.map(sectionFor);
    const isOnShow = section => suitesOnShow(listed).includes(section.suite);
    let isExpanded = new QueryModifier('suite').current().length > 0 || listedSections().some(section => isOnShow(section) && picked.has(section.suite));
    let renderedListing = null;
    let filterText = '';
    let refused = null;
    let sectionsElement = null;
    let renderedSections = [];
    let filterInput = null;
    let clearButton = null;
    let reloadTimer = null;

    const selectedIn = section => subSuitesInAddress(listed).get(section.suite) || [];
    const valueFor = (section, name) => `${section.suite}/${name}`;
    const ancestorsOf = (section, name) => {
        const ancestors = [];
        for (let node = section.nodes.get(name); node && node.parent; node = section.nodes.get(node.parent))
            ancestors.push(node.parent);
        return ancestors;
    };
    const isOpen = (section, name) => (filterText ? section.openWhileFiltering : section.openGroups).has(name);
    const isSectionOpen = section => (filterText ? section.isExpandedWhileFiltering : section.isExpanded);
    const isCounted = (section, name, on) => on.includes(name) || ancestorsOf(section, name).some(ancestor => on.includes(ancestor));
    const separate = (section, values) => values.filter(value => !ancestorsOf(section, value).some(ancestor => values.includes(ancestor)));
    const topmostWithSameTests = (section, name) => {
        let topmost = name;
        while (section.nodes.has(topmost) && section.nodes.get(topmost).sameTestsAsParent)
            topmost = section.nodes.get(topmost).parent;
        return topmost;
    };
    const isSectionOn = (section, on) => {
        if (!filterText)
            return isOnShow(section);
        return section.found.size > 0 && [...section.found].every(name => isCounted(section, name, on));
    };
    const isEverySuiteOn = () => listedSections().every(isOnShow);
    const loaded = () => listedSections().filter(section => section.list && !section.list.error);
    const maxSelected = () => Math.min(...loaded().map(section => section.list.max_selected));
    const countOnPage = (changed, after) => listedSections().filter(section => section === changed || isOnShow(section)).reduce((count, section) => count + separate(section, section === changed ? after : selectedIn(section)).length, 0);
    const isShown = section => (filterText ? section.found.size > 0 : true);
    const show = section => {
        if (!isOnShow(section))
            writeSuitesOnShow(listed, listed.filter(suite => suite === section.suite || isOnShow(sectionFor(suite))));
    };
    const clearMessages = () => {
        sections.forEach(section => {
            section.message = '';
        });
        refused = null;
    };
    const reloadSoon = () => {
        clearTimeout(reloadTimer);
        reloadTimer = setTimeout(callback, SUB_SUITE_RELOAD_DELAY);
    };
    const notesMarkup = notes => notes.length ? `<span class="text tiny" style="${SUB_SUITE_NOTE_STYLE}">(${notes.join(', ')})</span>` : '';

    const rowFor = (section, name, depth, on) => {
        const node = section.nodes.get(name);
        const hasChildren = Boolean(node && node.children.length);
        const isCovered = ancestorsOf(section, name).some(ancestor => on.includes(ancestor));
        const notes = [];
        if (!node)
            notes.push('not in the list');
        const onUnder = hasChildren ? on.filter(value => ancestorsOf(section, value).includes(name)).length : 0;
        if (onUnder && !isOpen(section, name))
            notes.push(`${onUnder} on`);
        if (refused && refused.section === section && refused.name === name)
            notes.push(`at most ${maxSelected()} on the page`);
        const lines = Array.from({length: depth}, (_, level) => `${SUB_SUITE_LINE} no-repeat calc(${level * SUB_SUITE_INDENT}px + 0.5em - 0.5px) 0 / 1px 100%`);

        return `<div class="input" data-sub-suite="${escapeHTML(name)}" title="${escapeHTML([name, ...notes].join(' · '))}" style="${SUB_SUITE_ROW_STYLE}; padding-left: ${depth * SUB_SUITE_INDENT}px${lines.length ? `; background: ${lines.join(', ')}` : ''}">
                <a class="link-button"${hasChildren ? ' data-expand' : ''} style="flex: none; width: 1em; text-align: center${hasChildren ? '' : '; visibility: hidden'}">${isOpen(section, name) ? '-' : '+'}</a>
                <label style="display: flex; align-items: baseline; min-width: 0; margin-left: 4px; white-space: nowrap"><span style="overflow: hidden; text-overflow: ellipsis">${escapeHTML(node ? node.label : name)}</span>${notesMarkup(notes)}</label>
                <label class="switch"${isCovered ? ' style="opacity: 0.5"' : ''}>
                    <input type="checkbox"${on.includes(name) || isCovered || onUnder ? ' checked' : ''}${isCovered ? ' disabled' : ''}>
                    <span class="slider"></span>
                </label>
            </div>`;
    };
    const treeRows = (section, names, depth, on) => names.map(name => {
        const row = rowFor(section, name, depth, on);
        return section.openGroups.has(name) ? row + treeRows(section, section.nodes.get(name).children, depth + 1, on) : row;
    }).join('');
    const filteredRows = (section, on) => {
        const rows = [];
        let rowCount = 0;
        const visit = (names, depth, isUnderFound) => names.forEach(name => {
            if (!isUnderFound && !section.foundAndAbove.has(name))
                return;
            if (++rowCount <= SUB_SUITE_FILTER_ROW_LIMIT)
                rows.push(rowFor(section, name, depth, on));
            if (section.openWhileFiltering.has(name))
                visit(section.nodes.get(name).children, depth + 1, isUnderFound || section.found.has(name));
        });
        visit(section.topLevel, 0, false);
        const notShown = rowCount - rows.length;
        return rows.join('') + (notShown > 0 ? `<div class="text tiny">${notShown.toLocaleString()} more: type more</div>` : '');
    };
    const rowsMarkup = section => {
        if (!section.list || section.list.error)
            return '';
        const on = selectedIn(section);
        const notListed = on.filter(name => !section.nodes.has(name)).map(name => rowFor(section, name, 0, on)).join('');
        if (!section.topLevel.length)
            return notListed && `${notListed}<div class="text tiny">${section.list.has_runs ? 'This suite has no sub-suites' : 'No sub-suites yet: they show as runs upload.'}</div>`;
        if (!isSectionOpen(section))
            return notListed;
        return notListed + (filterText ? filteredRows(section, on) : treeRows(section, section.topLevel, 0, on));
    };
    const elementsOf = section => {
        const element = sectionsElement ? sectionsElement.querySelector(`[data-section="${sections.indexOf(section)}"]`) : null;
        if (!element)
            return null;
        return {
            expander: element.querySelector('[data-section-expand]'),
            label: element.querySelector('[data-section-label]'),
            toggle: element.querySelector('[data-section-switch]'),
            box: element.querySelector('[data-box]'),
        };
    };
    const renderRows = section => {
        const elements = elementsOf(section);
        if (!elements)
            return;
        const on = selectedIn(section);
        if (elements.box) {
            const markup = rowsMarkup(section);
            DOM.inject(elements.box, markup);
            elements.box.style.display = markup ? '' : 'none';
        }
        if (elements.expander)
            elements.expander.textContent = isSectionOpen(section) ? '-' : '+';
        if (elements.toggle)
            elements.toggle.checked = isSectionOn(section, on);
        if (elements.label) {
            const notes = section.message ? [section.message] : [];
            const onUnder = isSectionOpen(section) ? 0 : on.filter(value => section.nodes.has(value)).length;
            if (onUnder)
                notes.push(`${onUnder} on`);
            DOM.inject(elements.label, `<span style="overflow: hidden; text-overflow: ellipsis">${escapeHTML(section.suite)}</span>${notesMarkup(notes)}`);
        }
    };
    const scrollToFirstOn = section => {
        const elements = elementsOf(section);
        const on = selectedIn(section);
        const firstOn = elements && elements.box ? [...elements.box.querySelectorAll('[data-sub-suite]')].find(row => on.includes(row.dataset.subSuite)) : null;
        if (firstOn)
            elements.box.scrollTop = firstOn.offsetTop - elements.box.clientHeight / 3;
    };
    const sectionMarkup = section => `<div data-section="${sections.indexOf(section)}">
            <div class="input" style="${SUB_SUITE_ROW_STYLE}; padding-left: ${SUB_SUITE_BOX_INSET}">
                <a class="link-button" data-section-expand style="flex: none; width: 1em; text-align: center${section.topLevel.length ? '' : '; visibility: hidden'}">${isSectionOpen(section) ? '-' : '+'}</a>
                <label data-section-label style="display: flex; align-items: baseline; min-width: 0; margin-left: 4px; white-space: nowrap"></label>
                <label class="switch">
                    <input type="checkbox" data-section-switch>
                    <span class="slider"></span>
                </label>
            </div>
            <div data-box style="position: relative; max-height: ${SUB_SUITE_LIST_HEIGHT}; overflow-y: auto; overscroll-behavior: contain; padding: 2px 4px; border: 1px solid var(--borderColorInlineElement); border-radius: 4px"></div>
        </div>`;
    const allMarkup = () => (filterText || !listed.length ? '' : `<div class="input" style="${SUB_SUITE_ROW_STYLE}; padding-left: ${SUB_SUITE_BOX_INSET}">
                <span style="flex: none; width: 1em"></span>
                <label style="min-width: 0; margin-left: 4px; white-space: nowrap">All</label>
                <label class="switch">
                    <input type="checkbox" data-all-switch>
                    <span class="slider"></span>
                </label>
            </div>`);
    const renderAll = () => {
        const toggle = sectionsElement ? sectionsElement.querySelector('[data-all-switch]') : null;
        if (toggle)
            toggle.checked = isEverySuiteOn();
    };
    const messagesMarkup = () => {
        const messages = listedSections().filter(section => section.list && section.list.error).map(section => `The sub-suites of ${escapeHTML(section.suite)} didn't load`);
        if (listedSections().some(section => !section.list))
            return `${messages.map(message => `<div class="text tiny">${message}</div>`).join('')}<div class="loader"><div class="spinner"></div></div>`;
        if (filterText && !loaded().some(isShown))
            messages.push('No sub-suite matches');
        else if (!filterText && loaded().length && !loaded().some(section => section.topLevel.length || selectedIn(section).length))
            messages.push(loaded().every(section => section.list.has_runs) ? `${listed.length === 1 ? 'This suite has' : 'These suites have'} no sub-suites` : 'No sub-suites yet: they show as runs upload.');
        return messages.map(message => `<div class="text tiny">${message}</div>`).join('');
    };
    const renderSections = () => {
        if (!sectionsElement)
            return;
        renderedSections = listedSections().filter(isShown);
        DOM.inject(sectionsElement, allMarkup() + renderedSections.map(sectionMarkup).join('') + messagesMarkup());
        renderedSections.forEach(renderRows);
        renderAll();
    };
    const update = () => {
        const shown = listedSections().filter(isShown);
        if (shown.length === renderedSections.length && shown.every((section, index) => section === renderedSections[index])) {
            shown.forEach(renderRows);
            renderAll();
        } else
            renderSections();
    };

    const find = section => {
        const lowered = filterText.toLowerCase();
        section.isExpandedWhileFiltering = true;
        section.found.clear();
        section.foundAndAbove.clear();
        section.openWhileFiltering.clear();
        section.nodes.forEach((node, name) => {
            if (!lowered || !name.toLowerCase().includes(lowered) || (node.parent && node.parent.toLowerCase().includes(lowered)))
                return;
            section.found.add(name);
            section.foundAndAbove.add(name);
            ancestorsOf(section, name).forEach(ancestor => {
                section.foundAndAbove.add(ancestor);
                section.openWhileFiltering.add(ancestor);
            });
        });
    };
    const setFilter = text => {
        if (text === filterText)
            return;
        filterText = text;
        clearMessages();
        sections.forEach(find);
        renderSections();
    };
    const turnOn = (section, name) => {
        const turningOn = topmostWithSameTests(section, name);
        const turningOff = selectedIn(section).filter(value => ancestorsOf(section, value).includes(turningOn));
        if (countOnPage(section, [...selectedIn(section).filter(value => !turningOff.includes(value)), turningOn]) > maxSelected()) {
            refused = {section, name};
            return false;
        }
        turningOff.forEach(value => modifier.remove(valueFor(section, value)));
        modifier.append(valueFor(section, turningOn));
        return true;
    };
    const turnOnFound = section => {
        const on = selectedIn(section);
        const turningOn = [...new Set([...section.found].filter(name => !isCounted(section, name, on)).map(name => topmostWithSameTests(section, name)))];
        const turningOff = on.filter(value => turningOn.some(name => ancestorsOf(section, value).includes(name)));
        const after = [...on.filter(value => !turningOff.includes(value)), ...turningOn];
        if (countOnPage(section, after) > maxSelected()) {
            section.message = `${section.found.size.toLocaleString()} found, at most ${maxSelected()} on the page`;
            return;
        }
        turningOff.forEach(value => modifier.remove(valueFor(section, value)));
        turningOn.forEach(name => modifier.append(valueFor(section, name)));
    };
    const turnOffFound = section => {
        const turnedOnForFound = new Set([...section.found].map(name => topmostWithSameTests(section, name)));
        selectedIn(section).filter(value => section.found.has(value) || turnedOnForFound.has(value)).forEach(value => modifier.remove(valueFor(section, value)));
    };

    const bodyRef = REF.createRef({
        onElementMount: () => render(),
    });
    const render = () => {
        const element = bodyRef.element;
        if (!element)
            return;
        sectionsElement = null;
        filterInput = null;
        clearButton = null;
        if (!isExpanded) {
            DOM.inject(element, '');
            return;
        }

        const hasFolders = loaded().some(section => section.topLevel.length);
        const filterRef = REF.createRef({
            onElementMount: (input) => {
                filterInput = input;
                input.value = filterText;
                input.oninput = () => {
                    if (clearButton)
                        clearButton.style.display = input.value ? '' : 'none';
                    setFilter(input.value.trim());
                };
            },
        });
        const clearRef = REF.createRef({
            onElementMount: (button) => {
                clearButton = button;
                button.onclick = () => {
                    if (!filterInput)
                        return;
                    filterInput.value = '';
                    button.style.display = 'none';
                    setFilter('');
                    filterInput.focus();
                };
            },
        });
        const sectionsRef = REF.createRef({
            onElementMount: (container) => {
                sectionsElement = container;
                container.onchange = (event) => {
                    if (event.target.type !== 'checkbox')
                        return;
                    clearMessages();
                    if (event.target.hasAttribute('data-all-switch')) {
                        clearTimeout(reloadTimer);
                        writeSuitesOnShow(listed, event.target.checked ? listed : [], {clearSubSuites: true});
                        followSuitesOnShow();
                        update();
                        callback();
                        return;
                    }
                    const sectionElement = event.target.closest('[data-section]');
                    if (!sectionElement)
                        return;
                    const section = sections[sectionElement.dataset.section];
                    const before = selectedIn(section);
                    const wasOnShow = isOnShow(section);
                    const row = event.target.closest('[data-sub-suite]');
                    if (row) {
                        const name = row.dataset.subSuite;
                        if (!event.target.checked)
                            before.filter(value => value === name || ancestorsOf(section, value).includes(name)).forEach(value => modifier.remove(valueFor(section, value)));
                        else if (!turnOn(section, name))
                            event.target.checked = false;
                    } else if (!filterText) {
                        before.forEach(value => modifier.remove(valueFor(section, value)));
                        if (!event.target.checked)
                            writeSuitesOnShow(listed, listed.filter(suite => suite !== section.suite && isOnShow(sectionFor(suite))));
                    } else if (event.target.checked)
                        turnOnFound(section);
                    else
                        turnOffFound(section);
                    const isChanged = !deepCompare(selectedIn(section), before);
                    if (event.target.checked && (isChanged || (!row && !filterText)))
                        show(section);
                    if (isOnShow(section) !== wasOnShow) {
                        clearTimeout(reloadTimer);
                        followSuitesOnShow();
                        callback();
                        return;
                    }
                    update();
                    if (isChanged)
                        reloadSoon();
                };
                container.onclick = (event) => {
                    const sectionElement = event.target.closest('[data-section]');
                    if (!sectionElement)
                        return;
                    const section = sections[sectionElement.dataset.section];
                    if (event.target.closest('[data-section-expand]')) {
                        if (filterText)
                            section.isExpandedWhileFiltering = !section.isExpandedWhileFiltering;
                        else
                            section.isExpanded = !section.isExpanded;
                        renderRows(section);
                        if (isSectionOpen(section))
                            scrollToFirstOn(section);
                        return;
                    }
                    const expander = event.target.closest('[data-expand]');
                    const row = expander ? expander.closest('[data-sub-suite]') : null;
                    if (!row)
                        return;
                    const name = row.dataset.subSuite;
                    const open = filterText ? section.openWhileFiltering : section.openGroups;
                    if (open.has(name))
                        open.delete(name);
                    else
                        open.add(name);
                    renderRows(section);
                };
                renderSections();
                sections.forEach(scrollToFirstOn);
            },
        });

        DOM.inject(element, `${hasFolders ? `<div class="input">
                    <input type="text" ref="${filterRef}" autocomplete="off" autocapitalize="none" required style="padding-right: calc(var(--formLabelPadding) + 1em)"/>
                    <label>Filter sub-suites</label>
                    <a class="link-button" ref="${clearRef}" title="Clear the filter" aria-label="Clear the filter" style="position: absolute; right: var(--formLabelPadding)${filterText ? '' : '; display: none'}">X</a>
                </div>` : ''}
            <div style="--smallSize: ${SUB_SUITE_ROW_SIZE}">
                <div ref="${sectionsRef}"></div>
            </div>`);
    };

    const expander = REF.createRef({
        onElementMount: (element) => {
            element.onclick = () => {
                isExpanded = !isExpanded;
                element.innerHTML = isExpanded ? '-' : '+';
                render();
                if (!isExpanded)
                    setFilter('');
            };
        },
    });

    const requestList = section => {
        if (section.isRequested)
            return;
        section.isRequested = true;
        fetch(`api/${escapeEndpoint(section.suite)}/sub-suites`).then(response => response.json()).then(json => {
            if (!Array.isArray(json.sub_suites))
                throw new Error(json.description);
            const addNodes = (children, parent) => children.map(child => {
                section.nodes.set(child.name, {
                    label: parent ? child.name.slice(parent.length + 1) : child.name,
                    sameTestsAsParent: Boolean(child.same_tests_as_parent),
                    parent,
                    children: addNodes(child.children || [], child.name),
                });
                return child.name;
            });
            section.topLevel = addNodes(json.sub_suites, null);
            section.list = json;
            selectedIn(section).forEach(name => ancestorsOf(section, name).forEach(ancestor => section.openGroups.add(ancestor)));
            find(section);
            render();
        }).catch(error => {
            section.list = {error: true};
            render();
            console.error(error);
        });
    };
    const followSuitesOnShow = suitesNow => {
        if (Array.isArray(suitesNow))
            listed = suitesNow;
        const listing = [listed, suitesOnShow(listed)];
        if (deepCompare(listing, renderedListing))
            return;
        renderedListing = listing;
        clearMessages();
        listedSections().forEach(requestList);
        update();
    };
    onSuitesChange.action(followSuitesOnShow);
    renderedListing = [listed, suitesOnShow(listed)];
    listedSections().forEach(requestList);

    return `<div style="font-size: var(--smallSize);">
            <a class="link-button text medium" ref="${expander}">${isExpanded ? '-' : '+'}</a>
            Sub-suite <br>
            <div ref="${bodyRef}"></div>
        </div>`;
}

function LimitSlider(callback, max = 50000, defaultValue = 5000) {
    const limitModifier = new QueryModifier('limit');
    const startingValue = limitModifier.current().length ? limitModifier.current()[limitModifier.current().length -1]:defaultValue;

    var numberRef = null;
    var sliderRef = null;

    numberRef = REF.createRef({
        state: startingValue,
        onElementMount: (element) => {
            element.onchange = () => {
                limitModifier.replace(element.value);
                sliderRef.setState(parseInt(element.value, 10));
                callback();
            }
        },
        onStateUpdate: (element, state) => {element.value = state;}
    });
    sliderRef = REF.createRef({
        state: startingValue,
        onElementMount: (element) => {
            element.oninput = () => {
                const newLimit = Math.ceil(element.value);
                limitModifier.replace(newLimit);
                numberRef.setState(newLimit);
                callback();
            }
        },
        onStateUpdate: (element, state) => {element.value = state;}
    });
    return `<div class="input">
            <label>Limit:</label>
            <input type="range" min="0" max="${max}" ref="${sliderRef}" style="background:var(--inverseColor)"></input>
            <input type="number" min="1" ref="${numberRef}" pattern="^[0-9]"></input>
        </div>`
}

function ConfigurationSelectors(callback) {
    refreshConfigurations();
    const elements = [
        {'query': 'platform', 'name': 'Platform'},
        {'query': 'version_name', 'name': 'Version Name'},
        {'query': 'style', 'name': 'Style'},
        {'query': 'model', 'name': 'Model'},
        {'query': 'architecture', 'name': 'Architecture'},
        {'query': 'flavor', 'name': 'Flavor'},
    ];
    const resetButtonRef = REF.createRef({});
    const resetEventStream = resetButtonRef.fromEvent('click');

    return `${elements.map(details => {
        const modifier = new QueryModifier(details.query);

        let ref = REF.createRef({
            state: configurations,
            onStateUpdate: (element, state) => {
                let candidates = new Set();
                state.forEach(configuration => {
                    if (configuration[details.query] == null)
                        return;
                    candidates.add(configuration[details.query]);
                });
                modifier.current().forEach(param => {
                    if (param === 'All')
                        return;
                    candidates.add(param);
                });

                let options = [...candidates];
                options.sort();
                options.unshift('All');

                let switches = {};

                let isExpanded = modifier.current().length > 0;
                let expander = REF.createRef({
                    onElementMount: (element) => {
                        element.onclick = () => {
                            isExpanded = !isExpanded;
                            element.innerHTML = isExpanded ? '-' : '+';

                            Array.from(element.parentNode.children).forEach(child => {
                                if (element == child)
                                    return;
                                child.style.display = isExpanded ? 'block' : 'none';
                            });
                            resetEventStream.action(resetSwitch);
                        }
                    },
                    onElementUnmount: () => {
                        resetEventStream.stopAction(resetSwitch);
                    },
                });

                const resetSwitch = () => {
                    Object.keys(switches).forEach(key => {
                        if (key === 'All')
                            switches[key].checked = true;
                        else
                            switches[key].checked = false;
                    });
                    modifier.remove();
                    callback();
                };


                DOM.inject(element, `<a class="link-button text medium" ref="${expander}">${isExpanded ? '-' : '+'}</a>
                    ${details.name} <br>
                    ${options.map(option => {
                        let isChecked = false;
                        if (option === 'All' && modifier.current().length === 0)
                            isChecked = true;
                        else if (option !== 'All' && modifier.current().indexOf(option) >= 0)
                            isChecked = true;

                        
                        let swtch = REF.createRef({
                            onElementMount: (element) => {
                                switches[option] = element;
                                element.onchange = () => {
                                    if (option === 'All') {
                                        if (!element.checked)
                                            return;
                                        Object.keys(switches).forEach(key => {
                                            if (key === 'All')
                                                return;
                                            switches[key].checked = false;
                                        });
                                        modifier.remove();
                                    } else if (element.checked) {
                                        switches['All'].checked = false;
                                        modifier.append(option);
                                    } else {
                                        modifier.remove(option);
                                        if (modifier.current().length === 0)
                                            switches['All'].checked = true;
                                    }
                                    callback();
                                };
                            }
                        });


                        return `<div class="input" ${isExpanded ? '' : `style="display: none;"`}>
                                <label>${escapeHTML(option)}</label>
                                <label class="switch">
                                    <input type="checkbox"${isChecked ? ' checked': ''} ref="${swtch}">
                                    <span class="slider"></span>
                                </label>
                            </div>`;
                    }).join('')}`);
            },
        });
        configurationsDefinedCallbacks.push(() => {
            ref.setState(configurations);
        });

        return `<div style="font-size: var(--smallSize);" ref="${ref}"></div>`;
    }).join('')}
    <button class="button" ref="${resetButtonRef}">Reset</button>
    `;
}

function CommitRepresentation(callback) {
    const ref = REF.createRef({
        state: {},
        onStateUpdate: (element, state) => {
            if (!Object.keys(state).length)
                return;

            DOM.inject(
                element,
                Object.keys(state).map(repository => {
                    const selector = REF.createRef({
                        state: state[repository].representation,
                        onElementMount: (element) => {
                            if (!element)
                                return;
                            element.onchange = () => {
                                CommitBank.setCommitRepresentation(repository, element.value);
                                callback();
                            }
                        },
                        onStateUpdate: (element, representation) => {
                            element.innerHTML = state[repository].candidates.map(candidate => {
                                if (candidate === representation)
                                    return `<option selected value="${candidate}">${candidate}</option>`;
                                return `<option value="${candidate}">${candidate}</option>`;
                            }).join('');
                        },
                    });
                    return `<div class="input">
                        <select required ref="${selector}">
                           ${state[repository].candidates.map(repr => `<option value="${repr}">${repr}</option>`).join('')}
                        </select>
                        <label style="color:var(--inverseColor)">${repository.charAt(0).toUpperCase() + repository.slice(1)}</label>
                    </div>`;
                }).join(''));
        },
    });
    CommitBank.commitRepresentations(json => {
        ref.setState(json);
    });

    const expander = REF.createRef({
        state: {isExpanded: false},
        onStateUpdate: (element, state) => {
            element.innerHTML = state.isExpanded ? '-' : '+';

            Array.from(element.parentNode.children).forEach(child => {
                if (element == child)
                    return;
                child.style.display = state.isExpanded ? 'block' : 'none';
            });
        },
    });
    expander.fromEvent('click').action(() => expander.setState({isExpanded: !expander.state.isExpanded}));

    return `<div class="item">
            <div style="font-size: var(--smallSize);">
                <a class="link-button text medium" ref="${expander}">+</a>
                Commit Representation
                <div class="list" ref="${ref}" style="display: none;">
                    <div class="item loader">
                        <div class="spinner"></div>
                    </div>
                </div>
            </div>
        </div>`;
}

export {Drawer, BranchSelector, suitesOnShow, SubSuiteSelector, subSuitesInAddress, ConfigurationSelectors, LimitSlider, CommitRepresentation, CommitSearchBar};
