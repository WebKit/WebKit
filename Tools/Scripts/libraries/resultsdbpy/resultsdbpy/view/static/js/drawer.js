// Copyright (C) 2019 Apple Inc. All rights reserved.
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
import {queryToParams, paramsToQuery, QueryModifier, } from '/assets/js/common.js';
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
    const defaultBranches = new Set(['trunk', 'master']);
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
    return elements.map(details => {
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

                let isExpanded = false;
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
                        }
                    }
                });

                DOM.inject(element, `<a class="link-button text medium" ref="${expander}">+</a>
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
                            },
                        });

                        return `<div class="input" ${isExpanded ? '' : `style="display: none;"`}>
                                <label>${option}</label>
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
    }).join('')
}

export {Drawer, BranchSelector, ConfigurationSelectors, LimitSlider};
