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
var origin = new URL("https://svn.webkit.org/");
var loadJavaScriptCoreFeatures = xhrPromise(new URL("/repository/webkit/trunk/Source/JavaScriptCore/features.json", origin));
var loadWebCoreFeatures = xhrPromise(new URL("/repository/webkit/trunk/Source/WebCore/features.json", origin));
</script>

<style>
.page {
    display: -webkit-flex;
    display: flex;
    -webkit-flex-wrap: wrap;
    flex-wrap: wrap;
    -webkit-justify-content: space-between;
    justify-content: space-between;
    box-sizing: border-box;
    width: 100%;
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
    color: #444444;
}

.page h2 {
    font-weight: 200;
    font-size: 3rem;
}

.page h3 {
    font-weight: 400;
    font-size: 2.5rem;
}

.page p {
    margin-bottom: 3rem;
}

#feature-list {
    display: inline-block;
    width: 66%;
    word-wrap: break-word;
}

/* Hide the internal links on search since they are unlikely to work. */
#search:required:valid + *  .internal-reference {
    display: none;
}

.feature-header {
    display: -webkit-flex;
    display: flex;
    -webkit-flex-direction: row;
    flex-direction: row;
}
.feature-header > h3:first-of-type {
    -webkit-flex-grow: 1;
    flex-grow: 1;
    margin: 0;
}

ul.features {
    padding: 0;
}

.features .feature {
    position: relative;
    display: block;
    background-color: #f9f9f9;
    border: 1px solid #dddddd;
    border-radius: 3px;
    padding: 1em;
    margin: 1em 0 !important;
    max-height: intrinsic;
    min-height: 3rem;
    overflow-y: hidden;
    cursor: pointer;
    -webkit-transition: background-color 0.3s ease-in;
    -moz-transition: background-color 0.3s ease-in;
    transition: background-color 0.3s ease-in;
}

.features .feature:hover {
    background-color: white;
}

.feature.opened {
    background-color: white;
    max-height: 120rem;
}

.feature-description + *,
.feature-description .feature-desc,
.feature-description .comment {
    display: none;
    margin: 0;
}

.feature.opened .feature-description + *,
.feature.opened .feature-desc,
.feature.opened .feature-description .comment {
    display: block;
    margin-bottom: 3rem;
}

.feature.opened .feature-description + *:last-child {
    margin-bottom: 0;
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

.feature-header {
    position: relative;
    padding-right: 3rem;
}

.feature-header h3 .internal-reference a {
    color: #999999;
    text-decoration: none;
    padding-left: 0.5em;
}

.feature-header h3 a {
    color: #444;
}

.feature-header h3 .internal-reference a:hover {
    color: inherit;
    text-decoration: underline;
}

.feature.is-hidden {
    display: none;
}

ul.feature-details {
    margin: 0;
}
.feature-statusItem {
    margin-right: 0.5em;
}

.feature-status {
    font-size: 2rem;
    display: inline-block;
    position: relative;
    min-width: 4em;
    text-align: right;
}

.feature-status,
.feature-status a {
    color: #999999;
}

.feature .status-marker {
    width: 0;
    height: 0;
    position: absolute;
    top: 0;
    left: 0;
    border-style: solid;
    border-width: 20px 20px 0 0;
    border-color: transparent transparent transparent transparent;
}

#status-filters .done,
.feature-status.done,
.feature-status.done a {
    color: #339900;
}

.status-marker.done {
    border-color: #339900 transparent transparent transparent;
}

#status-filters .in-development,
.feature-status.in-development,
.feature-status.in-development a {
    color: #f46c0e;
}

.status-marker.in-development {
    border-color: #f46c0e transparent transparent transparent;
}

#status-filters .no-active-development,
.feature-status.no-active-development,
.feature-status.no-active-development a {
    color: #5858D6;
}

.status-marker.no-active-development {
    border-color: #5858D6 transparent transparent transparent;
}

#status-filters .partial-support,
.feature-status.partial-support,
.feature-status.partial-support a {
    color: #548c8c;
}

.status-marker.partial-support {
    border-color: #548c8c transparent transparent transparent;
}

#status-filters .prototyping,
.feature-status.prototyping,
.feature-status.prototyping a {
    color: #007AFF;
}

.status-marker.prototyping {
    border-color: #007AFF transparent transparent transparent;
}


#status-filters .under-consideration,
.feature-status.under-consideration,
.feature-status.under-consideration a {
    color: #cc9d00;
}

.status-marker.under-consideration {
    border-color: #FFC500 transparent transparent transparent;
}

.feature-status.removed,
.feature-status.removed a {
    color: #999999;
}

.status-marker.removed {
    border-color: #999999 transparent transparent transparent;
}

.feature-filters {
    position: relative;
    top: 0;
    display: inline-block;

    width: -webkit-calc(33.33% - 3rem);
    width: -moz-calc(33.33% - 3rem);
    width: calc(33.33% - 3rem);
    margin-right: 3rem;
    font-size: 2rem;
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

.feature-filters ul {
    margin-top: 3rem;
}

.feature-filters ul li {
    margin-bottom: 1rem;
}

.feature-filters label > input {
    position: relative;
    top: -3px;
}

h3 a[name], .admin-bar h3 a[name] {
    top: initial;
    width: auto;
    display: inline-block;
    visibility: visible;
}

@media only screen and (max-width: 508px) {
    #feature-filters,
    #feature-list {
        width: 100%;
    }

    #feature-filters {
        border: 1px solid #dddddd;
        border-radius: 3px;
        background: #f6f6f6;
        padding: 1rem;
        box-sizing: border-box;
        margin-right: 0;
        margin-bottom: 3rem;
    }

    .feature-header h3 {
        font-size: 2rem;
    }

    .feature-status {
        font-size: 1.6rem;
        margin-top: 0.4rem;
    }
}

</style>
	<?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <div class="page feature-status-page" id="post-<?php the_ID(); ?>">
            <?php echo str_repeat('&nbsp;', 200);?>
			<h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>
            <form id="feature-filters" class="feature-filters">
                <h2>Filters</h2>
                <input type="text" id="search" placeholder="Search filter&hellip;" title="Filter the feature list." required>
                <ul id="status-filters">
                </ul>

            </form>

            <div id="feature-list">
            <h2>Features</h2>
            </div>

            <template id="success-template">
                <ul class="features" id="features-container"></ul>
                <p>Cannot find something? You can contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or contact the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list for questions.</p>
                <p>You can also <a href="coding/contributing.html">contribute to features</a> directly, the entire project is Open Source. To report bugs on existing features or check existing bug reports, see <a href="https://bugs.webkit.org">https://bugs.webkit.org</a>.</p>
            </template>
            <template id="error-template">
                <p>Error: unable to load the features list (<span id="error-message"></span>).</p>
                <p>If this is not resolved soon, please contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter or the <a href="https://lists.webkit.org/mailman/listinfo/webkit-help">webkit-help</a> mailing list.</p>
            </template>
        </div>

	<?php //comments_template(); ?>

	<?php endwhile; else: ?>

		<p>No posts.</p>

	<?php endif; ?>


<script>
function initializeStatusPage() {

    function sortAlphabetically(array) {
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

    function createFeatureView(featureObject) {
        function createLinkWithHeading(elementName, heading, linkText, linkUrl) {
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

        var slug = featureObject.name.toLowerCase().replace(/ /g, '-');
        if ("features" in featureObject) {
            container.setAttribute("id", "specification-" + slug);
        } else {
            container.setAttribute("id", "feature-" + slug);
        }

        if (window.location.hash && window.location.hash == "#" + container.getAttribute('id')) {
            container.className += " opened";
        }

        var cornerStatus = document.createElement('div');
        cornerStatus.className = "status-marker ";
        container.appendChild(cornerStatus);

        var descriptionContainer = document.createElement('div');
        descriptionContainer.className = "feature-description";

        var featureHeaderContainer = document.createElement('div');
        featureHeaderContainer.className = "feature-header";
        descriptionContainer.appendChild(featureHeaderContainer);

        var titleElement = document.createElement("h3");
        var anchorLinkElement = document.createElement("a");
        anchorLinkElement.href = "#" + container.getAttribute("id");
        anchorLinkElement.name = container.getAttribute("id");
        anchorLinkElement.textContent = featureObject.name;
        titleElement.appendChild(anchorLinkElement);

        // Add sub-feature here
        if (hasSpecificationObject) {
            var specification = featureObject.specification;
            var specSpan = createLinkWithHeading("span", null, specification.name, "#specification-" + specification.name.toLowerCase().replace(/ /g, '-'));
            specSpan.className = "internal-reference";
            titleElement.appendChild(specSpan);
        }

        featureHeaderContainer.appendChild(titleElement);

        if ("status" in featureObject) {
            var statusContainer = document.createElement("span");
            cornerStatus.className += statusClassName = featureObject.status.status.toLowerCase().replace(/ /g, '-');
            statusContainer.className = "feature-status " + statusClassName;
            if ("webkit-url" in featureObject) {
                var statusLink = document.createElement("a");
                statusLink.href = featureObject["webkit-url"];
                statusLink.textContent = featureObject.status.status;
                statusContainer.appendChild(statusLink);
            } else {
                statusContainer.textContent = featureObject.status.status;
            }

            featureHeaderContainer.appendChild(statusContainer);
        }

        if ("description" in featureObject) {
            var testDescription = document.createElement('p');
            testDescription.className = "feature-desc";
            testDescription.innerHTML = featureObject.description;
            descriptionContainer.appendChild(testDescription);
        }

        if ("comment" in featureObject) {
            if ("description" in featureObject) {
                var hr = document.createElement("hr");
                hr.className = 'comment';
                descriptionContainer.appendChild(hr);
            }
            var comment = document.createElement('p');
            comment.className = 'comment';
            comment.innerHTML = featureObject.comment;
            descriptionContainer.appendChild(comment);
        }

        container.appendChild(descriptionContainer);

        if (hasDocumentationLink || hasReferenceLink || hasContactObject) {
            var moreInfoList = document.createElement("ul");
            if (hasDocumentationLink) {
                var url = featureObject["documentation-url"];
                moreInfoList.appendChild(createLinkWithHeading("li", "Documentation", url, url));
            }

            if (hasReferenceLink) {
                var url = featureObject.url;
                moreInfoList.appendChild(createLinkWithHeading("li", "Reference", url, url));
            }

            if (hasSpecificationObject) {
                var specification = featureObject.specification;
                var li = createLinkWithHeading("li", "Parent feature", specification.name, "#specification-" + specification.name.toLowerCase().replace(/ /g, '-'));
                li.className = "internal-reference";
                moreInfoList.appendChild(li);
            }

            if (hasContactObject) {
                var li = document.createElement("li");
                li.textContent = "Contact: ";
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

            container.appendChild(moreInfoList);
        }

        if ("features" in featureObject && featureObject.features.length) {
            var internalLinkContainer = document.createElement("div");
            internalLinkContainer.className = "internal-reference sub-features";
            internalLinkContainer.textContent = "Includes: "
            // internalLinkContainer.appendChild(trackedFeatures);

            var list = document.createElement("ul");
            for (var feature of featureObject.features) {
                var link = document.createElement("a");
                link.textContent = feature.name;
                link.href = "#feature-" + feature.name.toLowerCase().replace(/ /g, '-');

                var li = document.createElement("li");
                li.appendChild(link);
                list.appendChild(li);
            }
            internalLinkContainer.appendChild(list);
            container.appendChild(internalLinkContainer);
        }

        return container;
    }

    function renderFeaturesAndSpecifications(featureLikeObjects) {
        var featureContainer = document.getElementById('features-container');
        for (var featureLikeObject of featureLikeObjects) {
            featureContainer.appendChild(createFeatureView(featureLikeObject));
        }
    }

    function initSearch(featuresArray) {
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

            }
        });

        var statusLength = statusFilters.length;

        for (var key in statusFilters) {
            var status = statusFilters[key];
            var entry = document.createElement("li");
            var label = document.createElement("label");
            var input = document.createElement("input");
            input.setAttribute('type','checkbox');
            input.setAttribute('value', key);
            input.addEventListener('change', search);
            label.appendChild(input);
            label.className = status.toLocaleLowerCase().replace(/ /g, '-');
            label.appendChild(document.createTextNode(" " + status));
            entry.appendChild(label);
            statusContainer.appendChild(entry);
        }

        filtersForm.addEventListener('click', function (e) {
            if ( filtersForm.className.indexOf('opened') !== -1 ) {
                filtersForm.className = filtersForm.className.replace(' opened','');
            } else filtersForm.className += " opened";
        });

        inputField.addEventListener('input', search);

        var inputs = [].slice.call(filtersForm.getElementsByTagName('input'));
        inputs.forEach(function (input,i) {
            input.addEventListener('click', function (e) {
                e.stopPropagation();
            });
        });

        function search(ev) {
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

    function searchFeatures(featuresArray, searchTerm, statusFilters) {
        featuresArray.forEach(function(feature) {
            var visible = isSearchMatch(feature, searchTerm) && isStatusFiltered(feature, statusFilters);

            if (visible && !feature.visible) {
                feature.el.className = 'feature';
            } else if (!visible && feature.visible) {
                feature.el.className = 'feature is-hidden';
            }

            feature.visible = visible;
        });
    }

    function isSearchMatch(feature, searchTerm) {
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

    function isStatusFiltered(feature, activeFilters) {
        if (activeFilters.length == 0)
            return true;
        if (feature.status === undefined)
            return false;
        if (activeFilters.indexOf(feature.status.status.toLowerCase()) != -1)
            return true;

        return false;
    }

    function displayFeatures(results) {
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
                var specificationObject = specificationsByName[feature.specification];
                specificationObject.features.push(feature);
                feature.specification = specificationObject;
            }
            feature.isSpecification = false;
            featuresByName[feature.name] = feature;
        }

        var everythingToShow = allFeatures.concat(allSpecifications);

        sortAlphabetically(everythingToShow);
        renderFeaturesAndSpecifications(everythingToShow);
        initSearch(everythingToShow);

        if (window.location.hash) {
            var hash = window.location.hash;
            window.location.hash = ""; // Change hash so navigation takes place
            window.location.hash = hash;
        }
    }

    function displayError(error) {
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

    Promise.all([loadJavaScriptCoreFeatures, loadWebCoreFeatures]).then(displayFeatures).catch(displayError);
}

document.addEventListener("DOMContentLoaded", initializeStatusPage);
</script>

<?php get_footer(); ?>