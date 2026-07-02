

// DOM Helpers
class DOMUtils
{
    static removeAllChildren(node)
    {
        while (node.lastChild)
             node.removeChild(node.lastChild);
    }
    
    static createDetails(summaryText)
    {
        let summary = document.createElement('summary');
        summary.textContent = summaryText;
    
        let details = document.createElement('details');
        details.appendChild(summary);
        
        details.addEventListener('keydown', function(event) {
            const rightArrowKeyCode = 39;
            const leftArrowKeyCode = 37;
            if (event.keyCode == rightArrowKeyCode)
                this.open = true;
            else if (event.keyCode == leftArrowKeyCode)
                this.open = false;
        }, false);

        return details;
    }

    // Create a details widget with the specified `summaryText` as the collapsed content.
    // The `contentBuilder` function is called (with `this` unbound) when the widget is
    // expanded. The DOM node returned by the builder is installed as the expanded
    // content. The content is built only once when the widget is first expanded.
    static createLazyDetails(summaryText, contentBuilder)
    {
        let details = document.createElement('details');

        let summary = document.createElement('summary');
        summary.textContent = summaryText;
        details.appendChild(summary);

        let content = document.createElement('div');
        let loading = document.createElement('p');
        loading.textContent = 'Loading...';
        content.appendChild(loading);
        details.appendChild(content);

        let contentLoaded = false;
        details.addEventListener('toggle', () => {
            if (details.open && !contentLoaded) {
                let loadedContent;
                try {
                    loadedContent = contentBuilder();
                } catch (e) {
                    loadedContent = document.createElement('p');
                    loadedContent.textContent = e;
                };
                content.firstChild.replaceWith(loadedContent);
                contentLoaded = true;
            }
        });
        return details;
    }

    // Place the domNode into a container div with a 'close' button.
    // The result can be added as a child to the Details section.
    static detailsPanelWith(domNode)
    {
        let panel = document.createElement('div');
        panel.className = 'panel';
        const closeButton = document.createElement('button');
        closeButton.textContent = 'x';
        closeButton.className = 'close-button';
        closeButton.addEventListener('click', function() {
            panel.parentElement.removeChild(panel);
        });
        panel.appendChild(closeButton)
        panel.appendChild(domNode);
        return panel;
    }

    static scrollIntoViewIfNeeded(element)
    {
        const rect = element.getBoundingClientRect();
        const isVisible = (
            rect.top >= 0 &&
            rect.left >= 0 &&
            rect.bottom <= window.innerHeight &&
            rect.right <= window.innerWidth
        );

        if (!isVisible) {
            element.scrollIntoView({
                behavior: 'smooth',
                block: 'nearest'
            });
        }
    }
};

// Heap Inspector Helpers
class HeapInspectorUtils
{
    static humanReadableSize(sizeInBytes)
    {
        var i = -1;
        if (sizeInBytes < 512)
            return sizeInBytes + 'B';
        var byteUnits = ['KB', 'MB', 'GB'];
        do {
            sizeInBytes = sizeInBytes / 1024;
            i++;
        } while (sizeInBytes > 1024);

        return Math.max(sizeInBytes, 0.1).toFixed(1) + byteUnits[i];
    }

    static addressForNode(node)
    {
        return node.wrappedAddress ? node.wrappedAddress : node.address;
    }

    static nodeName(snapshot, node)
    {
        if (node.type == "Internal")
            return 'Internal node';

        let result = node.className + ' @' + node.id + ' (' + HeapInspectorUtils.addressForNode(node) + ' ' + node.label + ')';
        if (node.gcRoot || node.markedRoot)
            result += ' (GC root—' + snapshot.reasonNamesForRoot(node.id).join(', ') + ')';
    
        return result;
    }

    static nodeNameAndIdOnly(node)
    {
        let nodeSpan = document.createElement('span');
        nodeSpan.innerHTML = node.className + `<a class="node-id" href="#">${node.id}</a>`
        let anchor = nodeSpan.querySelector('a');
        anchor.onclick = function(event) {
            event.preventDefault();
            inspector.exploreNodeId(node.id);
        };
        return nodeSpan;
    }

    static spanForNode(inspector, node, showPathButton)
    {
        let nodeSpan = document.createElement('span');

        if (node.type == "Internal") {
            nodeSpan.textContent = 'Internal node';
            return nodeSpan;
        }

        let wrappedAddressString = node.wrappedAddress ? `wrapped ${node.wrappedAddress}` : '';

        let nodeHTML = node.className + `<a class="node-id" href="#">${node.id}</a> <span class="node-address">cell ${node.address} ${wrappedAddressString}</span> <span class="node-size retained-size">(retains ${HeapInspectorUtils.humanReadableSize(node.retainedSize)})</span>`;
    
        if (node.label.length)
            nodeHTML += ` <span class="node-label">“${node.label}”</span>`;
    
        nodeSpan.innerHTML = nodeHTML;
        let anchor = nodeSpan.querySelector('a');
        anchor.onclick = function(event) {
            event.preventDefault();
            inspector.exploreNodeId(node.id);
        };

        if (node.gcRoot || node.markedRoot) {
            let gcRootSpan = document.createElement('span');
            gcRootSpan.className = 'node-gc-root';
            gcRootSpan.textContent = ' (GC root—' + inspector.snapshot.reasonNamesForRoot(node.id).join(', ') + ')';
            nodeSpan.appendChild(gcRootSpan);
        } else if (showPathButton) {
            let showAllPathsAnchor = document.createElement('button');
            showAllPathsAnchor.className = 'node-show-all-paths';
            showAllPathsAnchor.textContent = 'Show all paths';
            showAllPathsAnchor.addEventListener('click', (e) => {
                inspector.showAllPathsToNode(node);
            }, false);
            nodeSpan.appendChild(showAllPathsAnchor);
        }
        let dominators = this.dominatorIdsOfNode(node, inspector.snapshot);
        if (dominators.length > 0) {
            let mySpan = document.createElement('span');
            mySpan.textContent = ' dominators: [' + dominators.join(', ') + ']';
            nodeSpan.appendChild(mySpan);
        }
    
        return nodeSpan;
    }

    static dominatorIdsOfNode(node, snapshot)
    {
        let result = [];
        let here = node;
        let isFirst = true;
        while (here.id != 0) {
            if (!isFirst) result.push(here.id);
            isFirst = false;
            let dominatorId = here.dominatorNodeIdentifier;
            here = snapshot.nodeWithIdentifier(Number(dominatorId));
        }
        return result;
    }

    static spanForEdge(snapshot, edge)
    {
        let edgeSpan = document.createElement('span');
        edgeSpan.className = 'edge';
        edgeSpan.innerHTML = '<span class="edge-type">' + edge.type + '</span> <span class="edge-data">' + edge.data + '</span>';
        return edgeSpan;
    }

    static summarySpanForPath(inspector, path)
    {
        let pathSpan = document.createElement('span');
        pathSpan.className = 'path-summary';
    
        if (path.length > 0) {
            let pathLength = (path.length - 1) / 2;
            pathSpan.textContent = pathLength + ' step' + (pathLength > 1 ? 's' : '') + ' from ';
            pathSpan.appendChild(HeapInspectorUtils.spanForNode(inspector, path[0]), true);
        }

        return pathSpan;
    }

    static edgeName(edge)
    {
        return '⇒ ' + edge.type + ' ' + edge.data + ' ⇒';
    }
};


// Manages a list of heap snapshot nodes that can dynamically build the contents of an HTMLListElement.
class InstanceList
{
    constructor(listElement, snapshot, listGeneratorFunc)
    {
        this.listElement = listElement;
        this.snapshot = snapshot;
        this.nodeList = listGeneratorFunc(this.snapshot);
        this.entriesAdded = 0;
        this.initialEntries = 100;
        this.entriesQuantum = 50;
        this.showMoreItem = undefined;
    }
    
    buildList(inspector)
    {
        DOMUtils.removeAllChildren(this.listElement);
        if (this.nodeList.length == 0)
            return;

        let maxIndex = Math.min(this.nodeList.length, this.initialEntries);
        this.appendItemsForRange(inspector, 0, maxIndex);
        
        if (maxIndex < this.nodeList.length) {
            this.showMoreItem = this.makeShowMoreItem(inspector);
            this.listElement.appendChild(this.showMoreItem);
        }
        
        this.entriesAdded = maxIndex;
    }
    
    appendItemsForRange(inspector, startIndex, endIndex)
    {
        for (let index = startIndex; index < endIndex; ++index) {
            let instance = this.nodeList[index];
            let listItem = document.createElement('li');
            listItem.appendChild(HeapInspectorUtils.spanForNode(inspector, instance, true));
            this.listElement.appendChild(listItem);
        }

        this.entriesAdded = endIndex;
    }
    
    appendMoreEntries(inspector)
    {
        let numRemaining = this.nodeList.length - this.entriesAdded;
        if (numRemaining == 0)
            return;

        this.showMoreItem.remove();

        let fromIndex = this.entriesAdded;
        let toIndex = Math.min(this.nodeList.length, fromIndex + this.entriesQuantum);
        this.appendItemsForRange(inspector, fromIndex, toIndex);
        
        if (toIndex < this.nodeList.length) {
            this.showMoreItem = this.makeShowMoreItem(inspector);
            this.listElement.appendChild(this.showMoreItem);
        }
    }

    makeShowMoreItem(inspector)
    {
        let numberRemaining = this.nodeList.length - this.entriesAdded;
        
        let listItem = document.createElement('li');
        listItem.className = 'show-more';
        listItem.appendChild(document.createTextNode(`${numberRemaining} more `));

        let moreButton = document.createElement('button');
        moreButton.textContent = `Show ${Math.min(this.entriesQuantum, numberRemaining)} more`;

        let thisList = this;
        moreButton.addEventListener('click', function(e) {
            thisList.appendMoreEntries(inspector, 10);
        }, false);
        
        listItem.appendChild(moreButton);
        return listItem;
    }
};


class HeapSnapshotInspector
{
    constructor(containerElement, heapJSONData, filename)
    {
        this.containerElement = containerElement;
        this.resetUI();

        this.snapshot = new HeapSnapshot(1, heapJSONData, filename);
        
        this.buildRoots();
        this.buildAllObjectsByType();

        this.buildPathsToRootsOfType('Window');
        this.buildPathsToRootsOfType('HTMLDocument');
    }
    
    resetUI()
    {
        DOMUtils.removeAllChildren(this.containerElement);
        
        // All objects by type
        this.objectByTypeContainer = document.createElement('section');
        this.objectByTypeContainer.id = 'all-objects-by-type';
        let header = document.createElement('h1');
        header.textContent = 'All Objects by Type'
        this.objectByTypeContainer.appendChild(header);
        
        // Roots
        this.rootsContainer =  document.createElement('section');
        this.rootsContainer.id = 'roots';
        header = document.createElement('h1');
        header.textContent = 'Roots'
        this.rootsContainer.appendChild(header);

        // Important objects
        this.pathsToRootsContainer =  document.createElement('section');
        this.pathsToRootsContainer.id = 'paths-to-roots';
        header = document.createElement('h1');
        header.textContent = 'Important objects'
        this.pathsToRootsContainer.appendChild(header);

        // Details
        this.details =  document.createElement('section');
        this.details.id = 'details';
        header = document.createElement('h1');
        header.textContent = 'Details'
        this.details.appendChild(header);
        let clearAllButton = document.createElement('button');
        clearAllButton.id = 'details-clear-all';
        clearAllButton.innerText = 'Clear All';
        clearAllButton.addEventListener("click", () => {
            let header = this.details.childNodes[0];
            DOMUtils.removeAllChildren(this.details);
            this.details.appendChild(header);
        });
        header.appendChild(clearAllButton);

        this.containerElement.appendChild(this.details);
        this.containerElement.appendChild(this.pathsToRootsContainer);
        this.containerElement.appendChild(this.rootsContainer);
        this.containerElement.appendChild(this.objectByTypeContainer);

        // Cache for precomputed paths in the All Paths To... section.
        // Enables on-demand DOM node creation to cut down on memory use for
        // objects with a huge number of paths.
        this.nodePathDetailsWeakMap = new WeakMap()
    }

    dominatorInfo(className)
    {
        let dominators = {};
        let functions = HeapSnapshot.instancesWithClassName(this.snapshot, className);
        for (var node of functions) {
            let dominatorId = node.dominatorNodeIdentifier;
            let count = dominators[dominatorId];
            count = count === undefined ? 1 : count + 1;
            dominators[dominatorId] = count;
        }
        return dominators;
    }

    dominatorSummary(className)
    {
        let summary = document.createElement('section');
        summary.className = 'dominator-summary';
        let dominators = this.dominatorInfo(className);
        let entries = Object.entries(dominators).sort(([,a], [,b]) => b - a);  // Sort by value (descending)
        let i = 0;
        for (const [id, count] of entries) {
            let node = this.snapshot.nodeWithIdentifier(Number(id));
            let entry = document.createElement('p');
            entry.textContent = `${count} dominated by `;
            entry.appendChild(HeapInspectorUtils.nodeNameAndIdOnly(node));
            summary.appendChild(entry);
            i++;
            if (i > 50) {
                let ellipsis = document.createElement('p');
                ellipsis.textContent = '...';
                summary.appendChild(ellipsis);
                break;
            }
        }
        return summary;
    }

    widgetWithDominatorSummary(className)
    {
        let self = this;
        let count = this.snapshot._categories[className]?.count ?? 0;
        return DOMUtils.createLazyDetails(`Dominator summary of ${className} (${count} instances)`, function () {
            return self.dominatorSummary(className);
        });
    }

    // Produce a DOM node expandable into the shortest path from GC roots to the node with the given ID.
    widgetWithPathToNodeWithId(id)
    {
        let node = this.snapshot.nodeWithIdentifier(id);
        let shortestPath = this.snapshot.shortestGCRootPath(id).reverse();

        let details = DOMUtils.createDetails('Shortest path to ');
        let summary = details.firstChild;
        summary.appendChild(HeapInspectorUtils.spanForNode(this, node, true));
        summary.appendChild(document.createTextNode('—'));
        summary.appendChild(HeapInspectorUtils.summarySpanForPath(this, shortestPath));

        let pathList = document.createElement('ul');
        pathList.className = 'path';

        let isNode = true;
        let currItem = undefined;
        for (let item of shortestPath) {
            if (isNode) {
                currItem = document.createElement('li');
                currItem.appendChild(HeapInspectorUtils.spanForNode(this, item));
                pathList.appendChild(currItem);
            } else {
                currItem.appendChild(HeapInspectorUtils.spanForEdge(this.snapshot, item));
                currItem = undefined;
            }
            isNode = !isNode;
        }

        details.appendChild(pathList);
        return details;
    }

    widgetExpandableToImmedidatelyRetained(id, prefixText='')
    {
        let node = this.snapshot.nodeWithIdentifier(id);
        let self = this;
        let details = DOMUtils.createLazyDetails(prefixText, function() {
            let retained = self.snapshot.retainedNodes(id);
            let list = document.createElement('div');
            for (let each of retained.retainedNodes) {
                list.appendChild(self.widgetExpandableToImmedidatelyRetained(each.id));
            }
            return list;
        });
        let summary = details.firstChild;
        summary.appendChild(HeapInspectorUtils.spanForNode(this, node));
        return details;
    }

    widgetExpandableToImmedidateOwners(id, prefixText='')
    {
        let node = this.snapshot.nodeWithIdentifier(id);
        let self = this;
        let details = DOMUtils.createLazyDetails(prefixText, function() {
            let owners = self.snapshot.retainers(id);
            let list = document.createElement('div');
            for (let each of owners.retainers) {
                list.appendChild(self.widgetExpandableToImmedidateOwners(each.id));
            }
            return list;
        });
        let summary = details.firstChild;
        summary.appendChild(HeapInspectorUtils.spanForNode(this, node));
        return details;
    }

    // Add to the details area an explorer panel for a node with the given ID.
    exploreNodeId(id)
    {
        let retained = this.widgetWithPathToNodeWithId(id);
        let explorer = DOMUtils.detailsPanelWith(retained);
        explorer.appendChild(this.widgetExpandableToImmedidateOwners(id, 'Owners of '));
        explorer.appendChild(this.widgetExpandableToImmedidatelyRetained(id, 'Owned by '));
        this.details.appendChild(explorer);
        DOMUtils.scrollIntoViewIfNeeded(explorer);
    }

    // Add to the details area a widget with a summary of immediate dominators of instances of the given type.
    showDominatorsOfType(typeName)
    {
        let widget = this.widgetWithDominatorSummary(typeName);
        let panel = DOMUtils.detailsPanelWith(widget);
        this.details.appendChild(panel);
        DOMUtils.scrollIntoViewIfNeeded(panel);
    }

    buildAllObjectsByType()
    {
        let categories = this.snapshot._categories;
        let categoryNames = Object.keys(categories).sort();
    
        for (let categoryName of categoryNames) {
            let category = categories[categoryName];

            let details = DOMUtils.createDetails(`${category.className} (${category.count})`);
            
            let summaryElement = details.firstChild;
            let sizeElement = summaryElement.appendChild(document.createElement('span'));
            sizeElement.className = 'retained-size';
            sizeElement.textContent = ' ' + HeapInspectorUtils.humanReadableSize(category.retainedSize);

            let dominatorsButton = document.createElement('button');
            dominatorsButton.className = 'type-show-dominators';
            dominatorsButton.textContent = 'Dominators';
            dominatorsButton.addEventListener('click', (e) => {
                e.preventDefault();
                inspector.showDominatorsOfType(categoryName);
            }, false);
            summaryElement.appendChild(dominatorsButton);

            
            let instanceListElement = document.createElement('ul');
            instanceListElement.className = 'instance-list';
            
            let instanceList = new InstanceList(instanceListElement, this.snapshot, function(snapshot) {
                return HeapSnapshot.instancesWithClassName(snapshot, categoryName);
            });
            
            instanceList.buildList(this);

            details.appendChild(instanceListElement);
            this.objectByTypeContainer.appendChild(details);
        }
    }
    
    buildRoots()
    {
        let roots = this.snapshot.rootNodes();
        if (roots.length == 0)
            return;

        let groupings = roots.reduce(function(accumulator, node) {
            var key = node.className;
                if (!accumulator[key]) {
                  accumulator[key] = [];
                }
                accumulator[key].push(node);
                return accumulator;        
        }, {});
    
        let rootNames = Object.keys(groupings).sort();
    
        for (var rootClassName of rootNames) {
            let rootsOfType = groupings[rootClassName];
        
            rootsOfType.sort(function(a, b) {
                let addressA = HeapInspectorUtils.addressForNode(a);
                let addressB = HeapInspectorUtils.addressForNode(b);
                return (addressA < addressB) ? -1 : (addressA > addressB) ? 1 : 0;
            })

            let details = DOMUtils.createDetails(`${rootClassName} (${rootsOfType.length})`);

            let summaryElement = details.firstChild;
            let retainedSize = rootsOfType.reduce((accumulator, node) => accumulator + node.retainedSize, 0);
            let sizeElement = summaryElement.appendChild(document.createElement('span'));
            sizeElement.className = 'retained-size';
            sizeElement.textContent = ' ' + HeapInspectorUtils.humanReadableSize(retainedSize);

            let rootsTypeList = document.createElement('ul')
            rootsTypeList.className = 'instance-list';

            for (let root of rootsOfType) {
                let rootListItem = document.createElement('li');
                rootListItem.appendChild(HeapInspectorUtils.spanForNode(this, root, true));
                rootsTypeList.appendChild(rootListItem);
            }

            details.appendChild(rootsTypeList);

            this.rootsContainer.appendChild(details);
        }
    }
    
    buildPathsToRootsOfType(type)
    {
        let instances = HeapSnapshot.instancesWithClassName(this.snapshot, type);
        if (instances.length == 0)
            return;

        let detailsContainer = document.createElement('section')
        detailsContainer.className = 'path';

        for (var instance of instances) {
            let shortestPath = this.snapshot.shortestGCRootPath(instance.id).reverse();

            let details = DOMUtils.createDetails('');
            let summary = details.firstChild;
            summary.appendChild(HeapInspectorUtils.spanForNode(this, instance, true));
            summary.appendChild(document.createTextNode('—'));
            summary.appendChild(HeapInspectorUtils.summarySpanForPath(this, shortestPath));

            let pathList = document.createElement('ul');
            pathList.className = 'path';

            let isNode = true;
            let currItem = undefined;
            for (let item of shortestPath) {
                if (isNode) {
                    currItem = document.createElement('li');
                    currItem.appendChild(HeapInspectorUtils.spanForNode(this, item));
                    pathList.appendChild(currItem);
                } else {
                    currItem.appendChild(HeapInspectorUtils.spanForEdge(this.snapshot, item));
                    currItem = undefined;
                }
                isNode = !isNode;
            }
        
            details.appendChild(pathList);
            detailsContainer.appendChild(details);
        }

        this.pathsToRootsContainer.appendChild(detailsContainer);
    }

    populatePathDetailsOnDemand(pathDetails)
    {
        let pathNodes = this.nodePathDetailsWeakMap.get(pathDetails);
        if (!pathNodes) {
            pathDetails.appendChild(document.createTextNode('Error loading path: Path not cached in HeapSnapshotInspector.nodePathDetailsWeakMap'));
            return;
        }

        let pathList = document.createElement('ul');
        pathList.className = 'path';

        let isNode = true;
        let currItem = undefined;
        for (let item of pathNodes) {
            if (isNode) {
                currItem = document.createElement('li');
                currItem.appendChild(HeapInspectorUtils.spanForNode(this, item));
                pathList.appendChild(currItem);
            } else {
                currItem.appendChild(HeapInspectorUtils.spanForEdge(this.snapshot, item));
                currItem = undefined;
            }
            isNode = !isNode;
        }

        pathDetails.appendChild(pathList);
    }

    showAllPathsToNode(node)
    {
        let paths = this.snapshot._gcRootPaths(node.id);
    
        let details = DOMUtils.createDetails('');
        let summary = details.firstChild;
        summary.appendChild(document.createTextNode(`${paths.length} path${paths.length > 1 ? 's' : ''} to `));
        summary.appendChild(HeapInspectorUtils.spanForNode(this, node, false));

        let detailsContainer = document.createElement('section')
        detailsContainer.className = 'path';
    
        for (let path of paths) {
            let pathNodes = path.map((component) => {
                if (component.node)
                    return this.snapshot.serializeNode(component.node);
                return this.snapshot.serializeEdge(component.edge);
            }).reverse();

            let pathDetails = DOMUtils.createDetails('');
            let pathSummary = pathDetails.firstChild;
            pathSummary.appendChild(HeapInspectorUtils.summarySpanForPath(this, pathNodes));

            if (!this.nodePathDetailsWeakMap.get(pathDetails))
                this.nodePathDetailsWeakMap.set(pathDetails, pathNodes);
        
            pathDetails.addEventListener('toggle', (event) => {
                if (event.target.open)
                    return this.populatePathDetailsOnDemand(event.target);

                let pathSummary = pathDetails.firstChild;
                DOMUtils.removeAllChildren(event.target);
                pathDetails.appendChild(pathSummary);
            });

            detailsContainer.appendChild(pathDetails);
        }
    
        details.appendChild(detailsContainer);
        let panel = DOMUtils.detailsPanelWith(details);
        this.details.appendChild(panel);
        DOMUtils.scrollIntoViewIfNeeded(panel);
    }

};


function loadResults(dataString, filename)
{
    let inspectorContainer = document.getElementById('uiContainer');
    inspector = new HeapSnapshotInspector(inspectorContainer, dataString, filename)
}

function filenameForPath(filepath)
{
    var matched = filepath.match(/([^\/]+)(?=\.\w+$)/);
    if (matched)
        return matched[0];
    return filepath;
}

function hideDescription()
{
    document.getElementById('description').classList.add('hidden');
}

var inspector;

function setupInterface()
{
    // See if we have a file to load specified in the query string.
    var query_parameters = {};
    var pairs = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    var filename = "test-heap.json";

    for (var i = 0; i < pairs.length; i++) {
        var pair = pairs[i].split('=');
        query_parameters[pair[0]] = decodeURIComponent(pair[1]);
    }

    if ("filename" in query_parameters)
        filename = query_parameters["filename"];

    fetch(filename)
      .then(function(response) {
          if (response.ok)
              return response.text();
          throw new Error('Failed to load data file ' + filename);
      })
      .then(function(dataString) {
          loadResults(dataString, filenameForPath(filename));
          hideDescription();
          document.getElementById('uiContainer').style.display = 'block';
      });

    var drop_target = document.getElementById("dropTarget");

    drop_target.addEventListener("dragenter", function (e) {
        drop_target.className = "dragOver";
        e.stopPropagation();
        e.preventDefault();
    }, false);

    drop_target.addEventListener("dragover", function (e) {
        e.stopPropagation();
        e.preventDefault();
    }, false);

    drop_target.addEventListener("dragleave", function (e) {
        drop_target.className = "";
        e.stopPropagation();
        e.preventDefault();
    }, false);

    drop_target.addEventListener("drop", function (e) {
        drop_target.className = "";
        e.stopPropagation();
        e.preventDefault();

        for (var i = 0; i < e.dataTransfer.files.length; ++i) {
            var file = e.dataTransfer.files[i];

            var reader = new FileReader();
            reader.filename = file.name;
            reader.onload = function(e) {
                loadResults(e.target.result, filenameForPath(this.filename));
                hideDescription();
                document.getElementById('uiContainer').style.display = 'block';
            };

            reader.readAsText(file);
            document.title = "GC Heap: " + reader.filename;
        }
    }, false);
}

window.addEventListener('load', setupInterface, false);
