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
import {QueryModifier, paramsToQuery} from '/assets/js/common.js';

const LIMIT = 10;

function SearchBar(callback, suites) {
    let mouseCount = 0;
    let closeSearch = () => {};
    let inFlight = 0;

    const candidatesRef = REF.createRef({
        state: {candidates: {}, selected: null, displayed: false},
        onStateUpdate: (element, stateDiff, state) => {
            const selected = stateDiff.selected ? stateDiff.selected : state ? state.selected : null;
            const candidates = stateDiff.candidates ? stateDiff.candidates : state ? state.candidates : [];
            if (stateDiff.candidates) {
                let count = 0;
                DOM.inject(element, Object.keys(candidates).map(key => {
                    return candidates[key].map(test => {
                        const candidateRef = REF.createRef({
                            onElementMount: element => {
                                element.onmouseover = () => {
                                    ++mouseCount;
                                    candidatesRef.setState({selected: {suite: key, test: test}});
                                };
                                element.onmouseout = () => {mouseCount = mouseCount > 0 ? mouseCount - 1 : 0};
                                element.onclick = () => {
                                    callback({suite: key, test: test});
                                    closeSearch();
                                }
                            }
                        });
                        ++count;
                        return `<li ref="${candidateRef}" style="cursor: pointer;">${test} (${key})</li>`;
                    }).join('');
                }).join('') + `<div class="spinner tiny" ${inFlight > 0 ? '' : 'style="display: none;"'}></div>`);

                if (count)
                    element.style.display = 'block';
                else {
                    element.style.display = 'none';
                    mouseCount = 0;
                    stateDiff.selected = null;
                }
            }
            let index = 0;
            Object.keys(candidates).forEach(key => {
                candidates[key].forEach(test => {
                    if (selected && key === selected.suite && test === selected.test)
                        element.children[index].classList.add('selected');
                    else
                        element.children[index].classList.remove('selected');
                    ++index;
                });
            });
            element.children[index].style.display = inFlight > 0 ? 'block' : 'none';
        }
    });

    let candidates = {};
    let currentDispatch = Date.now();

    const nextSelected = (moveUp = false) => {
        const forIndexIn = moveUp ? (list, callback) => {
            for (let index = list.length - 1; index >= 0; --index) {
                if (!callback(index))
                    return false;
            }
            return true;
        } : (list, callback) => {
            for (let index = 0; index < list.length; ++index) {
                if (!callback(index))
                    return false;
            }
            return true;
        }
        let didFind = false;
        let boundry = null;

        const suites = Object.keys(candidates);
        forIndexIn(suites, suiteIndex => {
            const tests = candidates[suites[suiteIndex]];
            return forIndexIn(tests, testIndex => {
                if (!boundry)
                    boundry = {
                        suite: suites[suiteIndex],
                        test: tests[testIndex],
                    };
                if (!candidatesRef.state.selected)
                    return false;
                if (didFind) {
                    candidatesRef.setState({
                        selected: {
                            suite: suites[suiteIndex],
                            test: tests[testIndex],
                        },
                    });
                    return false;
                }
                if (candidatesRef.state.selected.suite == suites[suiteIndex] && candidatesRef.state.selected.test == tests[testIndex])
                    didFind = true;
                return true;
            });
        });
        if (!didFind)
            candidatesRef.setState({selected: boundry});
    };

    const inputRef = REF.createRef({
        onElementMount: element => {
            closeSearch = () => {
                currentDispatch = Date.now();
                element.value = '';
                candidates = {};
                candidatesRef.setState({candidates: {}, selected: null});
                mouseCount = 0;
                element.blur();
            }

            element.addEventListener('focus', () => {candidatesRef.setState({candidates: candidates});});
            element.addEventListener('blur', () => {
                if (!mouseCount)
                    candidatesRef.setState({candidates: {}});
            });
            element.addEventListener('keyup', event => {
                if (event.keyCode == 0x28 /* DOM_VK_DOWN */ || event.keyCode == 0x26 /* DOM_VK_UP */)
                    return;

                let myDispatch = Date.now();

                candidates = {};
                suites.forEach(suite => {
                    inFlight += 1;
                    fetch(`api/${suite}/tests?limit=${LIMIT}&test=${element.value}`).then(response => {
                        inFlight -= 1;
                        if (myDispatch < currentDispatch) {
                            candidatesRef.setState({});
                            return;
                        }
                        currentDispatch = Math.max(currentDispatch, myDispatch);

                        response.json().then(json => {
                            candidates[suite] = json;
                            if (json.length)
                                candidatesRef.setState({candidates: candidates});
                            else
                                candidatesRef.setState({});
                        });
                    }).catch(error => {
                        // If the load fails, log the error and continue
                        console.error(JSON.stringify(error, null, 4));
                        candidatesRef.setState({});
                    });
                });
            });
            element.addEventListener('keydown', event => {
                if (event.keyCode == 0x28 /* DOM_VK_DOWN */) {
                    nextSelected(false);

                } else if (event.keyCode == 0x26 /* DOM_VK_UP */) {
                    nextSelected(true);

                } else if (event.keyCode == 0x0D /* VK_RETURN */) {
                    if (candidatesRef.state.selected)
                        callback(candidatesRef.state.selected);
                    else {
                        let pairs = [];
                        Object.keys(candidates).forEach(suite => {
                            candidates[suite].forEach(test => {
                                pairs.push({
                                    suite: suite,
                                    test: test,
                                });
                            });
                        });
                        if (!pairs.length)
                            return;
                        callback(...pairs);
                    }
                    closeSearch();
                } else if (event.keyCode == 0x1B /* DOM_VK_ESCAPE */)
                    candidatesRef.setState({candidates: {}});
            });
            element.onpaste = (event) => {
                let ignoring = false;
                let tests = new Set();
                event.clipboardData.getData('text').split(/\s+/g).forEach(str => {
                    if (str === '[')
                        ignoring = true;
                    if (ignoring) {
                        if (str === ']')
                            ignoring = false;
                        return;
                    }
                    tests.add(str);
                });
                tests = [...tests];

                let pairs = [];
                let outstanding = suites.length;
                const query = paramsToQuery({test: tests});
                suites.forEach(suite => {
                    fetch(`api/${suite}/tests?${query}`).then(response => {
                        response.json().then(json => {
                            json.forEach(test => {
                                pairs.push({
                                    suite: suite,
                                    test: test,
                                });
                            });
                            outstanding -= 1;
                            if (outstanding <= 0) {
                                callback(...pairs);
                                closeSearch();
                            }
                        });
                    }).catch(error => {
                        // If the load fails, log the error and continue
                        console.error(JSON.stringify(error, null, 4));
                        candidatesRef.setState({});
                    });
                });
            }
        }
    });

    return `<div class="input">
            <input type="text" ref="${inputRef}" autocomplete="off" required/>
            <label>Search test</label>
        </div>
        <ul class="search-candidates" ref="${candidatesRef}" style="display: none;"></ul>`;
}

export {SearchBar};
