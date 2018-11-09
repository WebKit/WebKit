<?php
/**
 * Template Name: CSS Status Page
 **/
?>
<?php get_header(); ?>
<script>
function xhrPromise(url) {
    return new Promise(function(resolve, reject) {
        var xhrRequest = new XMLHttpRequest();
        xhrRequest.open('GET', url, true);
        xhrRequest.responseType = "json";

        xhrRequest.onload = function() {
            if (xhrRequest.status == 200 || xhrRequest.status == 0) {
                if (xhrRequest.response) {
                    resolve(xhrRequest.response);
                } else {
                    reject({ request: xhrRequest, url:url});
                }
            } else {
                reject({ request: xhrRequest, url:url});
            }
        };
        xhrRequest.onerror = function() {
            reject({ request: xhrRequest, url:url});
        };
        xhrRequest.send();
    });
}

var origin = new URL("https://<?php echo strpos(WP_HOST, "webkit.org") !== false ? "svn.webkit.org" : WP_HOST; ?>/");
var loadCSSProperties = xhrPromise(new URL("/repository/webkit/trunk/Source/WebCore/css/CSSProperties.json", origin));

</script>

<style>

.feature-status-page {
    animation: none !important; /* This animation can trigger a hit-testing bug, so remove it for now */
}

.page {
    display: -webkit-flex;
    display: flex;
    flex-direction: column;
    -webkit-justify-content: space-between;
    justify-content: space-between;
    box-sizing: border-box;
    width: 100%;
}

section.side-by-side {
    display: flex;
    display: -webkit-flex;
    flex: 1;
    -webkit-flex: 1;
}

sidebar {
    flex: 0 400px;
    margin-right: 3rem;
    font-size: 2rem;
    margin-left: 1rem;
}

section.primary {
    flex: 1;
    -webkit-flex: 1;
}

.sticky {
    position: -webkit-sticky;
    top: 0;
}

.page h1 {
    font-size: 4.2rem;
    font-weight: 200;
    line-height: 6rem;
    color: black;
    text-align: left;
    margin: 3rem auto;
    width: 100%;
}

.page h1 a {
    color: #444;
}

.page h2 {
    font-weight: 200;
    font-size: 3rem;
}

.page h3 {
    font-weight: 400;
    font-size: 2.2rem;
}

.property-count {
    text-align: right;
    color: #999;
}

.property-count p {
    margin: 0;
}

#property-list {
/*    word-wrap: break-word;*/
}

.property-header > h3:first-of-type {
    -webkit-flex-grow: 1;
    flex-grow: 1;
    margin: 0;
}

ul.properties {
    padding: 0;
}

.properties .property {
    position: relative;
    display: block;
    background-color: #f9f9f9;
    border: 1px solid #ddd;
    border-radius: 3px;
    padding: 1em;
    margin: 1em 0 !important;
    max-height: intrinsic;
    min-height: 3rem;
    overflow-y: hidden;
    cursor: pointer;
    transition: background-color 0.3s ease-in;
}

.properties .property:hover {
    background-color: white;
}

.property.opened {
    background-color: white;
    max-height: 120rem;
}

.property-description .toggleable {
    display: none;
}

.property.opened .property-description .toggleable {
    display: block;
    margin-top: 1rem;
}

.comment {
    font-size: smaller;
}

.more-info {
    margin-top: 0.5em;
    font-size: smaller;
}

.sub-features {
    font-size: 1.5rem;
    color: #555;
}

.sub-features ul {
    list-style: none;
    display: inline-block;
    padding: 0;
    margin: 0;
}

.sub-features li {
    display: inline;
}

.sub-features li:after {
    content: ", ";
}

.sub-features li:last-child:after {
    content: "";
}

ul.values {
    margin-left: 3em;
    margin-bottom: 0.5em;
    cursor: default;
}

.values li.hidden {
    color: #444;
}

.property-header {
    position: relative;
    padding-right: 3rem;
    display: -webkit-flex;
    display: flex;
    -webkit-flex-direction: row;
    flex-direction: row;
}

.property-header .toggle {
    display: inline-block;
    background: url('images/menu-down.svg') no-repeat 50%;
    background-size: 2rem;
    border: none;
    width: 2rem;
    height: 2rem;
    position: absolute;
    right: 0;
    top: 0.5rem;
    transition: transform 0.3s ease-out;
}

.property.opened .property-header .toggle {
    -webkit-transform: rotateX(-180deg);
    -moz-transform: rotateX(-180deg);
    transform: rotateX(-180deg);
}

.property-header h3 .spec-label {
    color: #999;
    text-decoration: none;
    font-weight: 200;
}

.property-header h3 .spec-label a {
    color: #999;
    text-decoration: none;
    font-weight: 200;
}

.spec-label::before {
    content: ' — ';
}
.property-header h3 a {
    color: #444;
}

.property-alias {
    font-size: smaller;
    color: #999;
}

.property-header p {
    margin-top: 0.5rem;
    margin-bottom: 0.5rem;
}

.value-alias {
    color: #999;
}

.value-status {
    color: #999;
}

.property.is-hidden {
    display: none;
}

ul.property-details {
    margin: 0;
}
.property-statusItem {
    margin-right: 0.5em;
}

.property-status {
    display: inline-block;
    position: relative;
    font-size: 2rem;
    min-width: 4em;
    text-align: right;
}

.property-status,
.property-status a {
    color: #999;
}

.property .status-marker {
    width: 0;
    height: 0;
    position: absolute;
    top: 0;
    left: 0;
    border-style: solid;
    border-width: 20px 20px 0 0;
    border-color: transparent transparent transparent transparent;
}

#status-filters .supported,
.property-status.supported,
.property-status.supported a {
    color: #339900;
}

.status-marker.supported {
    border-color: #339900 transparent transparent transparent;
}

#status-filters .in-development,
.property-status.in-development,
.property-status.in-development a {
    color: #f46c0e;
}

.status-marker.in-development {
    border-color: #f46c0e transparent transparent transparent;
}

#status-filters .no-active-development,
.property-status.no-active-development,
.property-status.no-active-development a {
    color: #5858D6;
}

.status-marker.no-active-development {
    border-color: #5858D6 transparent transparent transparent;
}

#status-filters .partial-support,
.property-status.partial-support,
.property-status.partial-support a {
    color: #548c8c;
}

.status-marker.partial-support {
    border-color: #548c8c transparent transparent transparent;
}

#status-filters .prototyping,
.property-status.prototyping,
.property-status.prototyping a {
    color: #007AFF;
}

.status-marker.prototyping {
    border-color: #007AFF transparent transparent transparent;
}

#status-filters .experimental,
.property-status.experimental,
.property-status.experimental a {
    color: #007AFF;
}

.status-marker.experimental {
    border-color: #007AFF transparent transparent transparent;
}

#status-filters .under-consideration,
.property-status.under-consideration,
.property-status.under-consideration a {
    color: #cc9d00;
}

.status-marker.under-consideration {
    border-color: #FFC500 transparent transparent transparent;
}

#status-filters .removed,
.property-status.removed,
.property-status.removed a {
    color: #7F7F7F;
}

.status-marker.removed {
    border-color: #7F7F7F transparent transparent transparent;
}

#status-filters .non-standard,
.property-status.non-standard,
.property-status.non-standard a {
    color: #8000FF;
}

.status-marker.non-standard {
    border-color: #8000FF transparent transparent transparent;
}

#status-filters .not-considering,
.property-status.not-considering,
.property-status.not-considering a {
    color: #7F7F7F;
}

.status-marker.not-considering {
    border-color: #7F7F7F transparent transparent transparent;
}

#status-filters .not-implemented,
.property-status.not-implemented,
.property-status.not-implemented a {
    color: #4C4C4C;
}

.status-marker.not-implemented {
    border-color: #4C4C4C transparent transparent transparent;
}

#status-filters .obsolete,
.property-status.obsolete,
.property-status.obsolete a {
    color: #804000;
}

.status-marker.obsolete {
    border-color: #804000 transparent transparent transparent;
}

.property-filters {
    position: relative;
    top: 0;
    margin-top: 0.5em;
}

#search {
    font-size: 2rem;
    padding: 1rem;
    border-radius: 3px;
    border: 1px solid #cccccc;
    width: 100%;
    margin-top: 1.5rem;
    box-sizing: border-box;
}

.property-filters ul {
    margin-top: 0.5rem;
    margin-bottom: 1.5rem;
}

.property-filters ul li {
    margin-bottom: 0.5rem;
}

.property-filters label > input {
    position: relative;
    top: -3px;
}

.prefixes {
    display: none;
}

#specifications {
    display: block;
    font-size: 1.6rem;
    border: 1px solid silver;
    width: calc(100% - 2rem);
    height: 3rem;
    margin: 1rem 2rem;
}

h3 a[name], .admin-bar h3 a[name] {
    top: initial;
    width: auto;
    display: inline-block;
    visibility: visible; /* Override visibility:hidden from themes/webkit/style.css */
}

@media only screen and (max-width: 508px) {
    #property-filters,
    #property-list {
        width: 100%;
    }

    #property-filters {
        border: 1px solid #ddd;
        border-radius: 3px;
        background: #f6f6f6;
        padding: 1rem;
        box-sizing: border-box;
        margin-right: 0;
        margin-bottom: 3rem;
    }

    .property-header h3 {
        font-size: 2rem;
    }

    .property-status {
        font-size: 1.6rem;
        margin-top: 0.4rem;
    }
}
</style>
    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <div class="page feature-status-page" id="post-<?php the_ID(); ?>">
            <?php echo str_repeat('&nbsp;', 200);?>
            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <section class="side-by-side">
                <sidebar>
                    <section class="sticky">
                        <form id="property-filters" class="property-filters">
                            <h2>Filters</h2>
                            <input type="text" id="search" placeholder="Search filter&hellip;" title="Filter the property list." required>
                            <h2>Filter by Status</h2>
                            <ul id="status-filters">
                            </ul>
                        </form>

                        <div class="prefixes">
                            <h2>Filter by Prefix</h2>
                            <ul id="prefix-filters">
                            </ul>
                        </div>
                    </section>
                </sidebar>

                <section class="primary">
                    <div id="property-list">
                        <div class="property-count">
                            <p><span id="property-count"></span> <span id="property-pluralize">properties</span></p>
                        </div>
                    </div>

                    <template id="success-template">
                        <ul class="properties" id="properties-container"></ul>

                        <p>Cannot find something? You can contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or contact the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list for questions.</p>
                        <p>You can also <a href="/contributing-code/">contribute to features</a> directly, the entire project is Open Source. To report bugs on existing features or check existing bug reports, see <a href="https://bugs.webkit.org">https://bugs.webkit.org</a>.</p>
                    </template>

                    <template id="error-template">
                        <p>Error: unable to load the features list (<span id="error-message"></span>).</p>
                        <p>If this is not resolved soon, please contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list.</p>
                    </template>

                </section>
            </section>
        </div>

    <?php //comments_template(); ?>

    <?php endwhile; else: ?>

        <p>No posts.</p>

    <?php endif; ?>


<script>
function initializeStatusPage() {

    const statusOrder = [
        'supported',
        'in-development',
        'under-consideration',
        'experimental',
        'non-standard',
        'not-considering',
        'not-implemented',
        'obsolete',
        'removed',
    ];

    const readableStatus = {
        'supported': 'Supported',
        'in-development': 'In Development',
        'under-consideration': 'Under Consideration',
        'experimental': 'Experimental',
        'non-standard': 'Non-standard',
        'not-considering': 'Not considering',
        'not-implemented': 'Not implemented',
        'obsolete': 'Obsolete',
        'removed': 'Removed',
    };

    function sortAlphabetically(array)
    {
        function replaceDashPrefix(name)
        {
            if (name[0] == '-')
                return 'Z' + name.slice(1);
            return name;
        }

        array.sort(function(a, b) {
            // Sort the prefixed properties to the end.
            var aName = replaceDashPrefix(a.name.toLowerCase());
            var bName = replaceDashPrefix(b.name.toLowerCase());

            var nameCompareResult = aName.localeCompare(bName);

            if (nameCompareResult)
                return nameCompareResult;

            // Status sort
            var aStatus = a.status != undefined ? a.status.status.toLowerCase() : '';
            var bStatus = b.status != undefined ? b.status.status.toLowerCase() : '';

            return aStatus.localeCompare(bStatus);
        });
    }

    function propertyNameAliases(propertyObject)
    {
        if ('codegen-properties' in propertyObject && 'aliases' in propertyObject['codegen-properties'])
            return propertyObject['codegen-properties'].aliases;

        return [];
    }

    function propertyLonghands(propertyObject)
    {
        if ('codegen-properties' in propertyObject && 'longhands' in propertyObject['codegen-properties'])
            return propertyObject['codegen-properties'].longhands;

        return [];
    }

    const prefixRegexp = /^(-webkit-|-epub-|-apple-)(.+)$/;

    function createPropertyView(categoryObject, propertyObject)
    {
        function createLinkWithHeading(elementName, heading, linkText, linkUrl)
        {
            var container = document.createElement(elementName);
            if (heading) {
                container.textContent = heading + ": ";
            }
            var link = document.createElement("a");
            link.textContent = linkText;
            link.href = linkUrl;
            container.appendChild(link);
            return container;
        }

        function appendValueWithLink(container, value, link)
        {
            if (link) {
                var anchor = document.createElement('a');
                anchor.href = link;
                anchor.textContent = value;
                container.appendChild(anchor);
                return;
            }

            container.textContent = value;
        }

        function makeTwitterLink(twitterHandle)
        {
            if (twitterHandle[0] == "@")
                twitterHandle = twitterHandle.substring(1);
            return "https://twitter.com/" + twitterHandle;
        }

        var hasSpecificationObject = "specification" in propertyObject;
        var specificationObject = propertyObject.specification;

        var container = document.createElement('li');

        container.className = "property";

        var slug = propertyObject.name.toLowerCase().replace(/ /g, '-');
        container.setAttribute("id", "property-" + slug);

        var cornerStatus = document.createElement('div');
        cornerStatus.className = "status-marker ";
        container.appendChild(cornerStatus);

        var descriptionContainer = document.createElement('div');
        descriptionContainer.className = "property-description";

        var featureHeaderContainer = document.createElement('div');
        featureHeaderContainer.className = "property-header";
        descriptionContainer.appendChild(featureHeaderContainer);

        var titleElement = document.createElement("h3");
        var anchorLinkElement = document.createElement("a");
        anchorLinkElement.href = "#" + container.getAttribute("id");
        anchorLinkElement.name = container.getAttribute("id");
        anchorLinkElement.textContent = propertyObject.name;
        titleElement.appendChild(anchorLinkElement);

        if (categoryObject) {
            if (specificationObject && ("url" in specificationObject || "obsolete-url" in specificationObject)) {
                var url = ("url" in specificationObject) ? specificationObject.url : specificationObject['obsolete-url'];
                var specSpan = createLinkWithHeading("span", null, categoryObject.shortname, specificationObject.url);
                specSpan.className = 'spec-label';
                titleElement.appendChild(specSpan);
            } else {
                var labelSpan = document.createElement("span");
                labelSpan.className = 'spec-label';
                labelSpan.textContent = categoryObject.shortname;
                titleElement.appendChild(labelSpan);
            }
        }

        featureHeaderContainer.appendChild(titleElement);

        var toggledContentContainer = document.createElement('div');
        toggledContentContainer.className = "toggleable";
        descriptionContainer.appendChild(toggledContentContainer);

        var aliases = propertyNameAliases(propertyObject);
        if (aliases.length) {
            var propertyAliasDiv = document.createElement('div');
            propertyAliasDiv.className = 'property-alias';
            propertyAliasDiv.textContent = 'Also supported as: ' + aliases.join(', ');
            toggledContentContainer.appendChild(propertyAliasDiv);
        }

        var longhands = propertyLonghands(propertyObject);
        if (longhands.length) {
            var longhandsDiv = document.createElement('div');
            longhandsDiv.className = 'longhands';

            longhandsDiv.textContent = 'Shorthand for ';

            for (var i in longhands) {
                if (i > 0)
                    longhandsDiv.appendChild(document.createTextNode(', '));
                var longhand = longhands[i];
                var longhandLink = document.createElement("a");
                longhandLink.href = "#property-" + longhand;
                longhandLink.textContent = longhand;
                longhandsDiv.appendChild(longhandLink);
            }

            toggledContentContainer.appendChild(longhandsDiv);
        }

        function collapsePrefixedValues(values)
        {
            var remainingValues = [];
            var prefixMap = {};

            for (var valueObj of values) {
                var valueName = valueObj.value;

                var result = prefixRegexp.exec(valueName);
                if (result) {
                    var unprefixed = result[2];
                    var unprefixedValue = findValueByName(values, unprefixed);
                    if (unprefixedValue) {
                        (prefixMap[unprefixedValue.value] = prefixMap[unprefixedValue.value] || []).push(valueName);
                        continue;
                    }
                }

                remainingValues.push(valueObj);
            }

            for (var prefixed in prefixMap) {
                var unprefixedValue = findValueByName(remainingValues, prefixed);
                unprefixedValue.aliases = prefixMap[prefixed];
            }

            return remainingValues;
        }

        if (propertyObject.values.length) {
            var valuesHeader = document.createElement("h4");
            valuesHeader.textContent = 'Supported Values:';
            toggledContentContainer.appendChild(valuesHeader);

            var valuesList = document.createElement("ul");
            valuesList.className = 'values';

            var values = collapsePrefixedValues(propertyObject.values);
            for (var valueObj of values) {
                var li = document.createElement("li");

                valueObj.el = li;

                var link = undefined;
                var valueAliases = undefined;
                var status = undefined;
                if ('aliases' in valueObj)
                    valueAliases = valueObj.aliases;

                if ('status' in valueObj)
                    status = valueObj.status;

                if ('url' in valueObj)
                    link = valueObj['url'];

                appendValueWithLink(li, valueObj.value, link);

                if (valueAliases) {
                    var span = document.createElement('span');
                    span.textContent = ' (' + valueAliases.join(', ') + ')';
                    span.className = 'value-alias';
                    li.appendChild(span);
                }

                if (status) {
                    var span = document.createElement('span');
                    span.textContent = ' (' + status + ')';
                    span.className = 'value-status';
                    li.appendChild(span);
                }

                valuesList.appendChild(li);
            }
            toggledContentContainer.appendChild(valuesList);
        }

        var statusContainer = document.createElement("span");
        cornerStatus.className += propertyObject.status.status;
        statusContainer.className = "property-status " + propertyObject.status.status;
        if ("webkit-url" in propertyObject) {
            var statusLink = document.createElement("a");
            statusLink.href = propertyObject["webkit-url"];
            statusLink.textContent = readableStatus[propertyObject.status.status];
            statusContainer.appendChild(statusLink);
        } else {
            statusContainer.textContent = readableStatus[propertyObject.status.status];
        }
        featureHeaderContainer.appendChild(statusContainer);

        var toggle = document.createElement('button');
        toggle.className = 'toggle';

        toggle.addEventListener('click', function (e) {
            container.classList.toggle('opened');
        });

        featureHeaderContainer.appendChild(toggle);

        if (specificationObject && "description" in specificationObject) {
            var testDescription = document.createElement('p');
            testDescription.className = "property-desc";
            testDescription.innerHTML = specificationObject.description;
            toggledContentContainer.appendChild(testDescription);
        }

        if (specificationObject && "comment" in specificationObject) {
            if ("description" in specificationObject) {
                var hr = document.createElement("hr");
                hr.className = 'comment';
                toggledContentContainer.appendChild(hr);
            }
            var comment = document.createElement('p');
            comment.className = 'comment';
            comment.innerHTML = specificationObject.comment;
            toggledContentContainer.appendChild(comment);
        }

        if (propertyObject.status && "comment" in propertyObject.status) {
            var comment = document.createElement('p');
            comment.className = 'comment';
            comment.innerHTML = propertyObject.status.comment;
            toggledContentContainer.appendChild(comment);
        }

        container.appendChild(descriptionContainer);
=
        function getMostSpecificProperty(categoryObject, specificationObject, attributeName)
        {
            // The url in the specification object is more specific, so use it if present.
            if (specificationObject && attributeName in specificationObject)
                return specificationObject[attributeName];

            return categoryObject[attributeName];
        }

        var hasReferenceLink = categoryObject && "url" in categoryObject;
        var hasDocumentationLink = (specificationObject && "documentation-url" in specificationObject) || (categoryObject && "documentation-url" in categoryObject);
        var hasContactObject = specificationObject && "contact" in specificationObject;

        if (hasDocumentationLink || hasReferenceLink || hasContactObject) {
            var moreInfoList = document.createElement("ul");
            moreInfoList.className = 'more-info';
            if (hasDocumentationLink) {
                // The url in the specification object is more specific, so use it if present.
                var url = getMostSpecificProperty(categoryObject, specificationObject, 'documentation-url');
                moreInfoList.appendChild(createLinkWithHeading("li", "Documentation", url, url));
            }

            if (hasReferenceLink) {
                var url = getMostSpecificProperty(categoryObject, specificationObject, 'url');
                moreInfoList.appendChild(createLinkWithHeading("li", "Reference", url, url));

                if ('obsolete-url' in specificationObject){
                    var url = specificationObject['obsolete-url'];
                    moreInfoList.appendChild(createLinkWithHeading("li", "Reference", url, url));
                }
            }

            if (hasContactObject) {
                var li = document.createElement("li");
                li.textContent = "Contact: ";
                var contactObject = specificationObject.contact;
                if (contactObject.twitter) {
                    li.appendChild(createLinkWithHeading("span", null, contactObject.twitter, makeTwitterLink(contactObject.twitter)));
                }
                if (contactObject.email) {
                    if (contactObject.twitter) {
                        li.appendChild(document.createTextNode(" - "));
                    }
                    var emailText = contactObject.email;
                    if (contactObject.name) {
                        emailText = contactObject.name;
                    }
                    li.appendChild(createLinkWithHeading("span", null, emailText, "mailto:" + contactObject.email));
                }
                moreInfoList.appendChild(li);
            }

            toggledContentContainer.appendChild(moreInfoList);
        }

        return container;
    }

    function canonicalizeIdentifier(identifier)
    {
        return identifier.toLocaleLowerCase().replace(/ /g, '-');
    }

    function renderSpecifications(categories, properties, selectedSpecifications)
    {
        var specificationsList = document.getElementById('specifications');
        specificationsList.addEventListener('change', function() { updateSearch(properties); });

        var selectedIndex = -1;
        var allCategories = Object.keys(categories).sort();

        for (var i = 0; i < allCategories.length; ++i) {
            var categoryKey = allCategories[i];
            var category = categories[categoryKey];
            categoryKey = canonicalizeIdentifier(categoryKey);

            var option = document.createElement("option");
            option.setAttribute('value', categoryKey);
            if (selectedSpecifications.indexOf(categoryKey) != -1)
                selectedIndex = i;

            option.appendChild(document.createTextNode(" " + category['shortname']));
            specificationsList.appendChild(option);
        }
        if (selectedIndex != -1)
            specificationsList.selectedIndex = selectedIndex;
    }

    function getPropertyCategory(propertyObject)
    {
        if ('specification' in propertyObject && 'category' in propertyObject.specification)
            return propertyObject.specification.category;

        return undefined;
    }

    function renderProperties(categories, propertyObjects)
    {
        var propertiesContainer = document.getElementById('properties-container');
        for (var propertyObject of propertyObjects) {
            var category = getPropertyCategory(propertyObject);
            propertiesContainer.appendChild(createPropertyView(categories[category], propertyObject));
        }
    }

    function convertToTitleCase(string)
    {
        return string.replace(/\w\S*/g, function(txt){
            return txt.charAt(0).toUpperCase() + txt.substr(1).toLowerCase();
        });
    }

    function initSearch(properties, categories)
    {
        var filtersForm = document.getElementById('property-filters');
        var statusContainer = document.getElementById('status-filters');
        var inputField = document.getElementById('search');
        var featuresEls = document.querySelectorAll('.properties > li');
        var statusFilters = {};

        properties.forEach(function(property, i) {
            property.el = featuresEls[i];
            property.visible = true;

            // FIXME: need status per value as well.
            if (property.status != undefined) {
                propertyStatusKey = property.status.status.toLocaleLowerCase();

                if (!statusFilters[propertyStatusKey])
                    statusFilters[propertyStatusKey] = property.status.status;

                if (statusOrder.indexOf(propertyStatusKey) == -1)
                    window.console.log('Status ' + propertyStatusKey + ' is not one of the predefined status keys ', statusOrder);
            }
        });

        var selectedStatuses = statusesFromURL();
        var selectedSpecs = specificationsFromURL();

        for (var key of statusOrder) {
            var status = statusFilters[key];
            var canonicalStatus = canonicalizeIdentifier(status);

            var entry = document.createElement("li");
            var label = document.createElement("label");
            var input = document.createElement("input");
            input.setAttribute('type','checkbox');
            input.setAttribute('value', canonicalStatus);
            if (selectedStatuses.indexOf(canonicalStatus) != -1)
                input.checked = true;
            input.className = 'status-checkbox';
            input.addEventListener('change', function() { updateSearch(properties); });
            label.appendChild(input);
            label.className = canonicalStatus;
            label.appendChild(document.createTextNode(" " + readableStatus[status]));
            entry.appendChild(label);
            statusContainer.appendChild(entry);
        }

        // Append the special "By Specification" checkbox
        {
            var entry = document.createElement("li");
            var label = document.createElement("label");
            var input = document.createElement("input");
            input.id = 'by-spec-checkbox';
            input.setAttribute('type','checkbox');
            if (selectedSpecs.length > 0)
                input.checked = true;
            input.addEventListener('change', function() { updateSearch(properties); });
            label.appendChild(input);
            label.appendChild(document.createTextNode(" By Specification:"));
            entry.appendChild(label);

            var specsList = document.createElement('select');
            specsList.className = 'specifications';
            specsList.id = 'specifications';
            entry.appendChild(specsList);

            statusContainer.appendChild(entry);
        }

        var prefixContainer = document.getElementById('prefix-filters');
        var prefixFilters = {};
        prefixFilters['prefix-supported-property'] = 'Property with and without prefix';
        prefixFilters['prefix-only-property'] = 'Prefixed-only Properties';
        prefixFilters['prefix-supported-value'] = 'Prefixed Values';
        prefixFilters['prefix-only-value'] = 'Prefixed-only Values';

        for (var key in prefixFilters) {
            var status = prefixFilters[key];
            var entry = document.createElement("li");
            var label = document.createElement("label");
            var input = document.createElement("input");
            input.setAttribute('type','checkbox');
            input.setAttribute('value', key);
            input.addEventListener('change', function() { updateSearch(properties); });
            label.appendChild(input);
            label.className = status.toLocaleLowerCase().replace(/ /g, '-');
            label.appendChild(document.createTextNode(" " + status));
            entry.appendChild(label);
            prefixContainer.appendChild(entry);
        }

        filtersForm.addEventListener('click', function (e) {
            if ( filtersForm.className.indexOf('opened') !== -1 ) {
                filtersForm.className = filtersForm.className.replace(' opened','');
            } else filtersForm.className += ' opened';
        });

        var searchTerm = searchTermFromURL();
        if (searchTerm.length) {
            inputField.value = searchTerm;
            inputField.placeholder = '';
        }
        inputField.addEventListener('input', function() { updateSearch(properties); });

        var inputs = [].slice.call(filtersForm.getElementsByTagName('input'));
        inputs.forEach(function (input,i) {
            input.addEventListener('click', function (e) {
                e.stopPropagation();
            });
        });

        renderSpecifications(categories, properties, selectedSpecs);
    }

    function getValuesOfCheckedItems(items)
    {
        var checkedValues = [];
        items.forEach(function(item,i) {
            if (item.checked)
                checkedValues.push(item.value);
        });

        return checkedValues;
    }

    function getValuesOfSelectedItems(select)
    {
        var selectedValues = [];

        if (select.selectedIndex != -1)
            selectedValues.push(select.options[select.selectedIndex].value);

        return selectedValues;
    }

    function selectedSpecifications()
    {
        var specificationsList = document.getElementById('specifications');
        if (!document.getElementById('by-spec-checkbox').checked) {
            specificationsList.disabled = true;
            return [];
        }
        specificationsList.disabled = false;
        return getValuesOfSelectedItems(specificationsList);
    }

    function updateSearch(properties)
    {
        var inputField = document.getElementById('search');
        var statusContainer = document.getElementById('status-filters');

        var searchTerm = inputField.value.trim().toLowerCase();
        var activeStatusFilters = getValuesOfCheckedItems([].slice.call(statusContainer.querySelectorAll('.status-checkbox')));

        var prefixContainer = document.getElementById('prefix-filters');
        var activePrefixFilters = getValuesOfCheckedItems([].slice.call(prefixContainer.getElementsByTagName('input')));

        var numVisible = searchProperties(properties, searchTerm, selectedSpecifications(), activeStatusFilters, activePrefixFilters);
        document.getElementById('property-pluralize').textContent = numVisible == 1 ? 'property' : 'properties';
        document.getElementById('property-count').textContent = numVisible;

        updateSpecsState();
        updateURL(searchTerm, selectedSpecifications(), activeStatusFilters, activePrefixFilters);
    }

    function updateSpecsState()
    {
        var specsEnabled = document.getElementById('by-spec-checkbox').checked;
        var specificationsList = document.getElementById('specifications');

        var radiobuttons = [].slice.call(specificationsList.getElementsByTagName('input'));
        radiobuttons.forEach(function(radiobutton,i) {
            radiobutton.disabled = !specsEnabled;
        });
    }

    function updateURL(searchTerm, selectedSpecifications, activeStatusFilters, activePrefixFilters)
    {
        var searchString = '';

        function appendDelimiter()
        {
            searchString += searchString.length ? '&' : '?';
        }

        if (searchTerm.length > 0) {
            appendDelimiter();
            searchString += 'search=' + encodeURIComponent(searchTerm);
        }

        if (activeStatusFilters.length) {
            appendDelimiter();
            searchString += 'status=' + activeStatusFilters.join(',');
        }

        if (selectedSpecifications.length) {
            appendDelimiter();
            searchString += 'specs=' + selectedSpecifications.join(',');
        }

        if (activePrefixFilters.length) {
            appendDelimiter();
            searchString += 'prefix=' + activePrefixFilters.join(',');
        }

        var current = window.location.href;
        window.location.href = current.replace(/#(.*)$/, '') + '#' + searchString;
    }

    function searchTermFromURL()
    {
        var search = window.location.search;
        var searchRegExp = /\#.*search=([^&]+)/;

        var result;
        if (result = window.location.href.match(searchRegExp))
            return decodeURIComponent(result[1]);

        return '';
    }

    function statusesFromURL()
    {
        var search = window.location.search;
        var statusRegExp = /\#.*status=([^&]+)/;

        var result;
        if (result = window.location.href.match(statusRegExp))
            return result[1].split(',');

        return [];
    }

    function specificationsFromURL()
    {
        var search = window.location.search;
        var specsRegExp = /\#.*specs=([^&]+)/;

        var result;
        if (result = window.location.href.match(specsRegExp))
            return result[1].split(',');

        return [];
    }

    function valueOrAliasIsPrefixed(valueObj)
    {
        if (prefixRegexp.exec(valueObj.value))
            return true;

        if ('alias' in valueObj) {
            for (var alias of valueObj.aliases) {
                if (prefixRegexp.exec(alias))
                    return true;
            }
        }

        return false;
    }

    function filterValues(propertyObject, searchTerm, categories, statusFilters)
    {
        for (var valueObj of propertyObject.values) {
            if (!valueObj.el)
                continue;

            var visible = false;
            visible = valueObj.value.toLowerCase().indexOf(searchTerm) !== -1;

            if (!visible) {
                for (var currValueObj of propertyObject.values) {
                    if (valueOrAliasIsPrefixed(currValueObj)) {
                        visible = true;
                        break;
                    }
                }
            }

            if (!visible) {
                for (var currValueObj of propertyObject.values) {
                    if (prefixRegexp.exec(currValueObj.value)) {
                        visible = true;
                        break;
                    }
                }
            }

            if (visible)
                valueObj.el.classList.remove('hidden');
            else
                valueObj.el.classList.add('hidden');
        }
    }

    function searchProperties(properties, searchTerm, categories, statusFilters, prefixFilters)
    {
        var visibleCount = 0;
        properties.forEach(function(propertyObject) {
            var matchesStatusSearch = isStatusFiltered(propertyObject, statusFilters);
            var matchesPrefixSearch = isPrefixFiltered(propertyObject, prefixFilters);

            var visible = propertyIsSearchMatch(propertyObject, searchTerm) && isCategoryMatch(propertyObject, categories) && matchesStatusSearch && matchesPrefixSearch;
            if (visible && !propertyObject.visible)
                propertyObject.el.className = 'property';
            else if (!visible && propertyObject.visible)
                propertyObject.el.className = 'property is-hidden';

            if (visible) {
                filterValues(propertyObject, searchTerm);
                ++visibleCount;
            }

            propertyObject.visible = visible;
        });

        return visibleCount;
    }

    function propertyIsSearchMatch(propertyObject, searchTerm)
    {
        if (searchTerm.length == 0)
            return true;

        if (propertyObject.name.toLowerCase().indexOf(searchTerm) !== -1)
            return true;

        if ('keywords' in propertyObject) {
            for (var keyword of propertyObject.keywords) {
                if (keyword.toLowerCase().indexOf(searchTerm) !== -1)
                    return true;
            }
        }

        for (var valueObj of propertyObject.values) {
            if (valueObj.value.toLowerCase().indexOf(searchTerm) !== -1)
                return true;
        }

        return false;
    }

    function getSpecificationCategory(propertyObject)
    {
        if ('specification' in propertyObject) {
            var specification = propertyObject.specification;
            if ('category' in specification) {
                return specification.category;
            }
        }
        return undefined;
    }

    function getSpecificationObsoleteCategory(propertyObject)
    {
        if ('specification' in propertyObject) {
            var specification = propertyObject.specification;
            if ('obsolete-category' in specification) {
                return specification['obsolete-category'];
            }
        }
        return undefined;
    }

    function isCategoryMatch(propertyObject, categories)
    {
        if (!categories.length)
            return true;

        var category;
        if (category = getSpecificationCategory(propertyObject)) {
            if (categories.indexOf(category) !== -1) {
                return true;
            }
        }

        if (category = getSpecificationObsoleteCategory(propertyObject)) {
            if (categories.indexOf(category) !== -1) {
                return true;
            }
        }
        return false;
    }

    function propertyOrAliasIsPrefixed(propertyObject)
    {
        if (prefixRegexp.exec(propertyObject.name))
            return true;

        for (var alias of propertyNameAliases(propertyObject)) {
            if (prefixRegexp.exec(alias))
                return true;
        }

        return false;
    }

    function isStatusFiltered(propertyObject, activeFilters)
    {
        if (activeFilters.length == 0)
            return true;
        if (propertyObject.status === undefined)
            return false;
        if (activeFilters.indexOf(propertyObject.status.status) !== -1)
            return true;

        return false;
    }

    function isPrefixFiltered(propertyObject, activeFilters)
    {
        if (activeFilters.length == 0)
            return true;

        if (activeFilters.indexOf('prefix-only-property') !== -1)
            return prefixRegexp.exec(propertyObject.name);

        if (activeFilters.indexOf('prefix-supported-property') !== -1)
            return propertyOrAliasIsPrefixed(propertyObject);

        if (activeFilters.indexOf('prefix-supported-value') !== -1) {
            for (var valueObj of propertyObject.values) {
                if (valueOrAliasIsPrefixed(valueObj))
                    return true;
            }
        }

        if (activeFilters.indexOf('prefix-only-value') !== -1) {
            for (var valueObj of propertyObject.values) {
                if (prefixRegexp.exec(valueObj.value))
                    return true;
            }
        }

        return false;
    }

    function findValueByName(values, name)
    {
        return values.find(function(element) {
            return element.value === name;
        })
    }

    function mergeProperties(unprefixedPropertyObj, prefixedPropertyObj)
    {
        (unprefixedPropertyObj['codegen-properties'].aliases = unprefixedPropertyObj['codegen-properties'].aliases || []).push(prefixedPropertyObj.name);

        for (var valueObj of prefixedPropertyObj.values) {
            if (!findValueByName(unprefixedPropertyObj.values, valueObj.value))
                prefixedPropertyObj.values.push(valueObj);
        }

        return unprefixedPropertyObj;
    }

    // Sometimes we have separate entries for -webkit-foo and foo.
    function collapsePrefixedProperties(properties)
    {
        function findPropertyByName(properties, name)
        {
            return properties.find(function(element) {
                return element.name === name;
            })
        }

        var remainingProperties = [];
        var prefixMap = {};

        for (var propertyObj of properties) {
            var propertyName = propertyObj.name;

            var result = prefixRegexp.exec(propertyName);
            if (result) {
                var unprefixed = result[2];
                var unprefixedProperty = findPropertyByName(properties, unprefixed);
                if (unprefixedProperty) {
                    mergeProperties(unprefixedProperty, propertyObj);
                    continue;
                }
            }

            remainingProperties.push(propertyObj);
        }

        return remainingProperties;
    }

    function canonicalizeValues(propertyObject)
    {
        var valueObjects = [];
        // Convert all values to objects.
        if ('values' in propertyObject) {
            for (var value of propertyObject.values) {
                if (typeof value === 'object')
                    valueObjects.push(value);
                else
                    valueObjects.push({ 'value' : value });
            }
        }
        propertyObject.values = valueObjects;
    }

    function canonicalizeStatus(propertyObject, categories)
    {
        // Inherit "status" from the cateogry if not explicitly specified.
        if (!('status' in propertyObject)) {
            var category = getSpecificationCategory(propertyObject)
            if (category) {
                var categoryObject = categories[category];
                if (categoryObject) {
                    if ('status' in categoryObject) {
                        propertyObject.status = {
                            'status' : categoryObject.status
                        };
                    }
                }
            }
        } else {
            // Convert all values to objects.
            if (typeof propertyObject.status === 'string')
                propertyObject.status = { 'status': propertyObject.status };
        }

        if (!('status' in propertyObject)) {
            propertyObject.status = {
                'status' : 'supported',
                'enabled-by-default' : true
            };
        } else if (!('status' in propertyObject.status))
            propertyObject.status.status = 'supported';

        propertyObject.status.status = canonicalizeIdentifier(propertyObject.status.status);
    }

    function renderContent(results)
    {
        var mainContent = document.getElementById("property-list");
        var successSubtree = document.importNode(document.getElementById("success-template").content, true);
        mainContent.appendChild(successSubtree);

        var properties = results[0]['properties'];
        var everythingToShow = [];

        var categories = results[0]['categories'];

        for (var property in properties) {
            var propertyObject = properties[property];
            propertyObject.name = property;

            canonicalizeValues(propertyObject);
            canonicalizeStatus(propertyObject, categories);

            everythingToShow.push(propertyObject);
        }

        everythingToShow = collapsePrefixedProperties(everythingToShow);
        sortAlphabetically(everythingToShow);

        renderProperties(categories, everythingToShow);

        initSearch(everythingToShow, categories);

        updateSearch(everythingToShow);
    }

    function displayError(error)
    {
        window.console.log('displayError', error)
        var mainContent = document.getElementById("property-list");
        var successSubtree = document.importNode(document.getElementById("error-template").content, true);

        var errorMessage = "Unable to load " + error.url;

        if (error.request.status !== 200) {
            errorMessage += ", status: " + error.request.status + " - " + error.request.statusText;
        } else if (!error.response) {
            errorMessage += ", the JSON file cannot be processed.";
        }

        successSubtree.querySelector("#error-message").textContent = errorMessage;

        mainContent.appendChild(successSubtree);
    }

    Promise.all([loadCSSProperties]).then(renderContent).catch(displayError);
}

document.addEventListener("DOMContentLoaded", initializeStatusPage);
</script>

<?php get_footer(); ?>
