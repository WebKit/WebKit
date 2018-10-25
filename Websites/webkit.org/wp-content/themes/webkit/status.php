<?php
/**
 * Template Name: Status Page
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
            if (xhrRequest.status == 200) {
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
var loadJavaScriptCoreFeatures = xhrPromise(new URL("/repository/webkit/trunk/Source/JavaScriptCore/features.json", origin));
var loadWebCoreFeatures = xhrPromise(new URL("/repository/webkit/trunk/Source/WebCore/features.json", origin));
</script>

<style>

.page h1 {
    font-size: 4.2rem;
    font-weight: 500;
    line-height: 6rem;
    margin: 3rem auto;
    width: 100%;
    text-align: center;
    color: #333333;
}

.page h1 a {
    color: inherit;
}

.feature-status-page {
    padding-bottom: 3rem;
}

.feature-status-page p {
    max-width: 920px;
    margin: 0 auto 3rem;
}

.feature-filters {
    background-color: #ffffff;
    width: 100vw;
    left: 50%;
    position: relative;
    transform: translate(-50vw, 0);
    box-sizing: border-box;
    margin-bottom: 3rem;
    border: 1px solid #DDDDDD;
    border-left: none;
    border-right: none;
}

.feature-filters form {
    padding-top: 3rem;
    padding-bottom: 3rem;
    display: flex;
    flex-wrap: wrap;
}

input[type=text].search-input {
    margin-bottom: 1rem;
    width: 100%;
    flex: 1;
}

.feature-filters li {
    display: inline-block;
}

.feature-status label,
.feature-filters label {
    display: table-cell;
    padding: 0.5rem 1rem;
    border-style: solid;
    border-width: 1px;
    border-radius: 3px;
    cursor: pointer;
    float: right;
    line-height: 1;
    font-size: 1.6rem;
}

.status-filters label {
    margin-left: 1rem;
}

.status-filters {
    list-style: none;
    display: inline-block;
    text-align: right;
    flex: 2;
    flex-grow: 1;
}

.filter-toggle:checked + .filter-status {
    color: #ffffff;
}

.filter-status,
.feature-status {
    color: #999999;
    border-color: #999999;
}

.feature-status a {
    color: inherit;
}

.filter-status,
.status-marker {
    border-color: #999999;
}
.filter-toggle:checked + .filter-status {
    background-color: #999999;
}

.supported {
    color: #339900;
    border-color: #339900;
}

.filter-toggle:checked + .supported {
    background-color: #339900;
}

.supported-in-preview {
    color: #66149f;
    border-color: #66149f;
}

.filter-toggle:checked + .supported-in-preview {
    background-color: #66149f;
}

.in-development {
    color: #f46c0e;
    border-color: #f46c0e;
}
.filter-toggle:checked + .in-development {
    background-color: #f46c0e;
}

.no-active-development {
    color: #5858D6;
    border-color: #5858D6;
}

.filter-toggle:checked + .no-active-development {
    background-color: #5858D6;
}

.partially-supported  {
    color: #548c8c;
    border-color: #548c8c;
}

.filter-toggle:checked + .partially-supported {
    background-color: #548c8c;
}

.prototyping {
    color: #007AFF;
    border-color: #007AFF;
}

.filter-toggle:checked + .prototyping {
    background-color: #007AFF;
}

.under-consideration {
    color: #c27870;
    border-color: #c27870;
}

.filter-toggle:checked + .under-consideration {
    background-color: #c27870;
}

.feature.is-hidden {
    display: none;
}

.features {
    max-width: 920px;
    margin: 0 auto 3rem;
    border-bottom: 1px solid #e4e4e4;

}

.feature-count {
    max-width: 920px;
    margin: 0 auto 3rem;

    text-align: right;
    color: #999;
}

.feature-count p {
    margin: 0;
}

.feature {
    border-color: transparent;
    border-width: 1px;
    border-style: solid;
    border-top-color: #e4e4e4;
    padding: 0.5rem;
    line-height: 1.618;
    transition: background-color 0.3s ease-out;
}

.feature-header {
    font-weight: 400;
    font-size: 2.5rem;
    display: flex;
}

.feature-header h3 {
    flex: 1;
    flex-grow: 2;
    padding-right: 1rem;
    box-sizing: border-box;
}

.feature-header h3 a {
    padding-right: 1rem;
}

.feature-header .feature-status {
    flex: 2;
    text-align: right;
    font-size: 2rem;
}

.feature-container.status-marker {
    border-left-width: 3px;
    border-left-style: solid;
    padding: 0.5rem 0 0.5rem 1rem;
}

.feature-header a[name] {
    color: #444444;
}

.feature-header .internal-reference {
    display: inline-block;
    font-size: 1.6rem;
    font-weight: 600;
    white-space: nowrap;
}

.feature-header .internal-reference a {
    color: #999999;
}

.feature-header:after {
    position: relative;
    width: 2rem;
    height: 2rem;
    right: 0;
    top: 0.5rem;
    margin-left: 1rem;
    transition: transform 0.3s ease-out;
}

.feature-details {
    display: none;
    width: 50%;
}

.feature.opened {
    background: #ffffff;
    border-left-color: #e4e4e4;
    border-right-color: #e4e4e4;
}

.feature.opened .feature-details {
    display: block;
}

.feature h4 {
    color: #999999;
    font-weight: 600;
    margin-top: 1rem;
    margin-bottom: 0;
}

.feature .moreinfo {
    list-style: none;
    display: flex;
    width: 100%;
}

.feature .moreinfo li {
    flex-grow: 1;
}

.feature .moreinfo .contact {
    text-align: right;
}

.feature .feature-desc {
    color: #444444;
}

.feature .comment {
    color: #666666;
    font-style: italic;
}

.sub-features {
    font-size: 1.5rem;
    color: #555;
}

.sub-features ul {
    list-style: none;
    padding: 0;
    margin: 0;
}

.sub-features li {
    display: inline-block;
    white-space: nowrap;
}

.sub-features li:after {
    content: ", ";
    white-space: pre;
}

.sub-features li:last-child:after {
    content: "";
}

@media only screen and (max-width: 1000px) {
    .feature-details {
        width: 100%;
    }
}

@media only screen and (max-width: 508px) {
    #feature-filters,
    #feature-list {
        width: 100%;
    }

    #feature-filters {
        padding-left: 2rem;
        padding-right: 2rem;
    }

    .feature-header h3 {
        font-size: 2rem;
        padding-right: 0.5rem;
    }

    .feature-status {
        font-size: 1.6rem;
        margin-top: 0.4rem;
        float: left;
    }

    .feature-header:after {
        width: 1rem;
        height: 1rem;
        background-size: 1rem;
        top: 1rem;
    }

    .feature h3 {
        font-size: 2rem;
        padding-top: 4rem;
    }

    .feature-header .feature-status {
        font-size: 1.6rem;
        position: absolute;
        text-align: left;
    }

    .feature .moreinfo {
        flex-wrap: wrap;
    }

    .feature .moreinfo .contact {
        text-align: left;
    }

    .status-filters {
        text-align: left;
        flex-basis: 100%;
    }

    .status-filters label {
        margin-left: 0;
        margin-right: 1rem;
    }
}

h3 a[name], .admin-bar h3 a[name] {
    top: initial;
    width: auto;
    display: inline-block;
    visibility: visible;
}


</style>
	<?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <div id="content">
        <div class="page feature-status-page" id="post-<?php the_ID(); ?>">
			<h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="feature-filters">
                <form id="feature-filters" class="page-width">
                    <input type="text" id="search" class="search-input" placeholder="Search features&hellip;" title="Filter the feature list." required>
                    <ul id="status-filters" class="status-filters"></ul>
                </form>
            </div>

            <div id="feature-list">
                <div class="feature-count">
                    <p><span id="feature-count"></span> <span id="feature-pluralize">features</span></p>
                </div>

            </div>

            <template id="success-template">
                <ul class="features" id="features-container"></ul>
                <p>Cannot find something? You can contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or contact the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list for questions.</p>
                <p>You can also <a href="/contributing-code/">contribute to features</a> directly, the entire project is Open Source. To report bugs on existing features or check existing bug reports, see <a href="https://bugs.webkit.org">https://bugs.webkit.org</a>.</p>
            </template>
            <template id="error-template">
                <p>Error: unable to load the features list (<span id="error-message"></span>).</p>
                <p>If this is not resolved soon, please contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list.</p>
            </template>
        </div>
        </div>

	<?php //comments_template(); ?>

	<?php endwhile; else: ?>

		<p>No posts.</p>

	<?php endif; ?>


<script>
function initializeStatusPage() {

    const statusOrder = [
        'under consideration',
        'prototyping',
        'in development',
        'supported in preview',
        'partially supported',
        'supported',
        'deprecated',
        'removed',
        'not considering'
    ];

    function sortAlphabetically(array)
    {
        array.sort(function(a, b){
            var aName = a.name.toLowerCase();
            var bName = b.name.toLowerCase();

            var nameCompareResult = aName.localeCompare(bName);

            if ( nameCompareResult )
                return nameCompareResult;

            // Status sort
            var aStatus = a.status != undefined ? a.status.status.toLowerCase() : '';
            var bStatus = b.status != undefined ? b.status.status.toLowerCase() : '';

            return aStatus.localeCompare(bStatus);
        });
    }

    function createFeatureView(featureObject)
    {

        function createLinkWithHeading(elementName, heading, linkText, linkUrl) {
            var container = document.createElement(elementName);
            if (heading) {
                var h4 = document.createElement('h4');
                h4.textContent = heading;
                container.appendChild(h4);
            }
            var link = document.createElement("a");
            link.textContent = linkText;
            link.href = linkUrl;
            if (linkText == linkUrl)
                link.textContent = link.hostname + "…";
            container.appendChild(link);
            return container;
        }

        function makeTwitterLink(twitterHandle) {
            if (twitterHandle[0] == "@")
                twitterHandle = twitterHandle.substring(1);
            return "https://twitter.com/" + twitterHandle;
        }

        var container = document.createElement('li');
        var hasDocumentationLink = "documentation-url" in featureObject;
        var hasReferenceLink = "url" in featureObject;
        var hasContactObject = "contact" in featureObject;
        var hasSpecificationObject = "specification" in featureObject;

        container.addEventListener('click', function (e) {
            if ( container.className.indexOf('opened') !== -1 ) {
                container.className = container.className.replace(' opened','');
            } else container.className += " opened";
        });

        container.className = "feature";

        var slug = canonicalizeIdentifier(featureObject.name);
        if ("features" in featureObject) {
            container.setAttribute("id", "specification-" + slug);
        } else {
            container.setAttribute("id", "feature-" + slug);
        }

        if (window.location.hash && window.location.hash == "#" + container.getAttribute('id')) {
            container.className += " opened";
        }

        var featureContainer = document.createElement('div');
        featureContainer.className = "feature-container status-marker";

        var featureHeaderContainer = document.createElement('div');
        featureHeaderContainer.className = "feature-header";
        featureContainer.appendChild(featureHeaderContainer);

        var titleElement = document.createElement("h3");
        var anchorLinkElement = document.createElement("a");
        anchorLinkElement.href = "#" + container.getAttribute("id");
        anchorLinkElement.name = container.getAttribute("id");
        anchorLinkElement.textContent = featureObject.name;
        titleElement.appendChild(anchorLinkElement);

        // Add sub-feature here
        if (hasSpecificationObject) {
            var specification = featureObject.specification;
            var specSpan = createLinkWithHeading("h4", null, specification.name, "#specification-" + specification.name.toLowerCase().replace(/ /g, '-'));
            specSpan.className = "internal-reference";
            titleElement.appendChild(specSpan);
        }

        featureHeaderContainer.appendChild(titleElement);

        if ("status" in featureObject) {
            var statusContainer = document.createElement("div");
            var statusClassName = canonicalizeIdentifier(featureObject.status.status);
            featureContainer.className += " " + statusClassName;
            statusContainer.className = "feature-status " + statusClassName;
            var statusLabel = document.createElement("label");

            if ("webkit-url" in featureObject) {
                var statusLink = document.createElement("a");
                statusLink.href = featureObject["webkit-url"];
                statusLink.textContent = featureObject.status.status;
                statusLabel.appendChild(statusLink);
            } else {
                statusLabel.textContent = featureObject.status.status;
            }

            statusContainer.appendChild(statusLabel);
            featureHeaderContainer.appendChild(statusContainer);
        }

        var featureDetails = document.createElement('div');
        featureDetails.className = 'feature-details';

        if ("description" in featureObject) {
            var textDescription = document.createElement('p');
            textDescription.className = "feature-desc";
            textDescription.innerHTML = featureObject.description;
            featureDetails.appendChild(textDescription);
        }

        if ("comment" in featureObject) {
            var comment = document.createElement('p');
            comment.className = 'comment';
            comment.innerHTML = featureObject.comment;
            featureDetails.appendChild(comment);
        }

        if ("features" in featureObject && featureObject.features.length) {
            var internalLinkContainer = document.createElement("div");
            internalLinkContainer.className = "internal-reference sub-features";
            var internalHeading = document.createElement("h4");
            internalHeading.textContent = "Includes";
            internalLinkContainer.appendChild(internalHeading);

            var list = document.createElement("ul");
            for (var feature of featureObject.features) {
                var link = document.createElement("a");
                link.textContent = feature.name;
                link.href = "#feature-" + canonicalizeIdentifier(feature.name);

                var li = document.createElement("li");
                li.appendChild(link);
                list.appendChild(li);
            }
            internalLinkContainer.appendChild(list);
            featureDetails.appendChild(internalLinkContainer);
        }

        if (hasDocumentationLink || hasReferenceLink || hasContactObject) {
            var moreInfoList = document.createElement("ul");
            moreInfoList.className = 'moreinfo';
            if (hasDocumentationLink) {
                var url = featureObject["documentation-url"];
                moreInfoList.appendChild(createLinkWithHeading("li", "Documentation", url, url));
            }

            if (hasReferenceLink) {
                var url = featureObject.url;
                moreInfoList.appendChild(createLinkWithHeading("li", "Reference", url, url));
            }

            if (hasContactObject) {
                var li = document.createElement("li");
                li.className = "contact";
                var contactHeading = document.createElement("h4");
                contactHeading.textContent = "Contact";
                li.appendChild(contactHeading);

                if (featureObject.contact.twitter) {
                    li.appendChild(createLinkWithHeading("span", null, featureObject.contact.twitter, makeTwitterLink(featureObject.contact.twitter)));
                }
                if (featureObject.contact.email) {
                    if (featureObject.contact.twitter) {
                        li.appendChild(document.createTextNode(" - "));
                    }
                    var emailText = featureObject.contact.email;
                    if (featureObject.contact.name) {
                        emailText = featureObject.contact.name;
                    }
                    li.appendChild(createLinkWithHeading("span", null, emailText, "mailto:" + featureObject.contact.email));
                }
                moreInfoList.appendChild(li);
            }

            featureDetails.appendChild(moreInfoList);
        }

        featureContainer.appendChild(featureDetails);
        container.appendChild(featureContainer);

        return container;
    }

    function canonicalizeIdentifier(identifier)
    {
        return identifier.toLocaleLowerCase().replace(/ /g, '-');
    }


    function renderFeaturesAndSpecifications(featureLikeObjects)
    {
        var featureContainer = document.getElementById('features-container');
        for (var featureLikeObject of featureLikeObjects) {
            featureContainer.appendChild(createFeatureView(featureLikeObject));
        }
    }

    function initSearch(featuresArray)
    {
        var filtersForm = document.getElementById('feature-filters');
        var statusContainer = document.getElementById('status-filters');
        var inputField = document.getElementById('search');
        var featuresEls = document.querySelectorAll('.features > li');
        var statusFilters = {};

        featuresArray.forEach(function(feature, i) {
            feature.el = featuresEls[i];
            feature.visible = true;

            if (feature.status != undefined) {
                featureStatusKey = feature.status.status.toLocaleLowerCase();

                if (!statusFilters[featureStatusKey])
                    statusFilters[featureStatusKey] = feature.status.status;

                if (statusOrder.indexOf(featureStatusKey) == -1)
                    window.console.log('Status ' + featureStatusKey + ' is not one of the predefined status keys ', statusOrder);

            }
        });

        for (var key of statusOrder) {
            if (statusFilters[key] == undefined)
                continue;

            var statusLabel = statusFilters[key];
            var statusId = canonicalizeIdentifier(statusLabel);
            var entry = document.createElement("li");
            var label = document.createElement("label");
            var input = document.createElement("input");

            input.setAttribute('type','checkbox');
            input.setAttribute('value', key);
            input.setAttribute('id', 'toggle-' + statusId);
            input.className = 'filter-toggle';
            input.addEventListener('change', function() { updateSearch(featuresArray); });


            label.className = "filter-status " + statusId;
            label.setAttribute('for', 'toggle-' + statusId);
            label.appendChild(input);
            label.appendChild(document.createTextNode(" " + statusLabel));

            entry.appendChild(label);

            statusContainer.appendChild(entry);
        }

        filtersForm.addEventListener('click', function (e) {
            if ( filtersForm.className.indexOf('opened') !== -1 ) {
                filtersForm.className = filtersForm.className.replace(' opened','');
            } else filtersForm.className += " opened";
        });

        var searchTerm = searchTermFromURL();
        if (searchTerm.length) {
            inputField.value = searchTerm;
            inputField.placeholder = '';
        }
        inputField.addEventListener('input', function() { updateSearch(featuresArray); });


        var inputs = [].slice.call(filtersForm.getElementsByTagName('input'));
        inputs.forEach(function (input,i) {
            input.addEventListener('click', function (e) {
                e.stopPropagation();
            });
        });

        function search(ev)
        {
            var searchTerm = inputField.value.trim().toLowerCase();
            var activeStatusFilters = [];
            var checkboxes = [].slice.call(statusContainer.getElementsByTagName('input'));
            checkboxes.forEach(function(checkbox,i) {
                if ( checkbox.checked )
                    activeStatusFilters.push(checkbox.value);
            });

            searchFeatures(featuresArray, searchTerm, activeStatusFilters);
        }
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

    function updateSearch(properties)
    {
        var inputField = document.getElementById('search');
        var statusContainer = document.getElementById('status-filters');

        var searchTerm = inputField.value.trim().toLowerCase();
        var activeStatusFilters = getValuesOfCheckedItems([].slice.call(statusContainer.querySelectorAll('.filter-toggle')));

        var numVisible = searchFeatures(properties, searchTerm, activeStatusFilters);
        document.getElementById('feature-pluralize').textContent = numVisible == 1 ? 'feature' : 'features';
        document.getElementById('feature-count').textContent = numVisible;

        updateURL(searchTerm, activeStatusFilters);
    }

    function searchFeatures(features, searchTerm, statusFilters)
    {
        var visibleCount = 0;
        features.forEach(function(featureObject) {
            var matchesStatusSearch = isStatusFiltered(featureObject, statusFilters);

            var visible = isSearchMatch(featureObject, searchTerm) && matchesStatusSearch;
            if (visible && !featureObject.visible)
                featureObject.el.className = 'feature';
            else if (!visible && featureObject.visible)
                featureObject.el.className = 'feature is-hidden';

            if (visible) {
                // filterValues(featureObject, searchTerm);
                ++visibleCount;
            }

            featureObject.visible = visible;
        });

        return visibleCount;
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

    function isSearchMatch(feature, searchTerm)
    {
        if (feature.name.toLowerCase().indexOf(searchTerm) !== -1)
            return true;
        if ("keywords" in feature) {
            for (var keyword of feature.keywords) {
                if (keyword.toLowerCase().indexOf(searchTerm) !== -1)
                    return true;
            }
        }
        return false;
    }

    function isStatusFiltered(feature, activeFilters)
    {
        if (activeFilters.length == 0)
            return true;
        if (feature.status === undefined)
            return false;
        if (activeFilters.indexOf(feature.status.status.toLowerCase()) != -1)
            return true;

        return false;
    }

    function filterValues(featureObject, searchTerm, statusFilters)
    {
        for (var valueObj of featureObject.values) {
            if (!valueObj.el)
                continue;

            var visible = false;
            visible = valueObj.value.toLowerCase().indexOf(searchTerm) !== -1;

            if (visible)
                valueObj.el.classList.remove('hidden');
            else
                valueObj.el.classList.add('hidden');
        }
    }

    function displayFeatures(results)
    {
        var mainContent = document.getElementById("feature-list");
        var successSubtree = document.importNode(document.getElementById("success-template").content, true);
        mainContent.appendChild(successSubtree);

        var allSpecifications = [];
        for (var i in results) {
            allSpecifications = allSpecifications.concat(results[i].specification);
        }
        var specificationsByName = {}
        for (var specification of allSpecifications) {
            specification.features = [];
            specification.isSpecification = true;
            specificationsByName[specification.name] = specification;
        }

        var allFeatures = [];
        for (var i in results) {
            allFeatures = allFeatures.concat(results[i].features);
        }
        var featuresByName = {};
        for (var feature of allFeatures) {
            if ('specification' in feature) {
                var featureSpecification = feature.specification;
                var specificationObject = specificationsByName[featureSpecification];
                if (specificationObject != undefined) {
                    specificationObject.features.push(feature);
                    feature.specification = specificationObject;
                } else {
                    feature.specification = {
                        name: featureSpecification
                    };
                }
            }
            feature.isSpecification = false;
            featuresByName[feature.name] = feature;
        }

        var everythingToShow = allFeatures.concat(allSpecifications);

        sortAlphabetically(everythingToShow);

        renderFeaturesAndSpecifications(everythingToShow);

        initSearch(everythingToShow);

        updateSearch(everythingToShow);

        if (window.location.hash.length) {
            var hash = window.location.hash;
            window.location.hash = ""; // Change hash so navigation takes place
            window.location.hash = hash;
        }
    }

    function displayError(error)
    {
        var mainContent = document.getElementById("feature-list");
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

    function updateURL(searchTerm, activeStatusFilters)
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

        if (searchString.length) {
            var current = window.location.href;
            window.location.href = current.replace(/\??#(.*)$/, '') + '#' + searchString;
        }

    }


    Promise.all([loadJavaScriptCoreFeatures, loadWebCoreFeatures]).then(displayFeatures).catch(displayError);
}

document.addEventListener("DOMContentLoaded", initializeStatusPage);
</script>

<?php get_footer(); ?>