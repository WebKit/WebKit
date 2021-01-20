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

var origin = new URL("<?php echo strpos(WP_HOST, "webkit.org") !== false ? "https://svn.webkit.org" : WP_HOME; ?>/");
var loadCSSProperties = xhrPromise(new URL("/repository/webkit/trunk/Source/WebCore/css/CSSProperties.json", origin));

</script>

<style>
:root {
    --feature-rule-color: hsl(0, 0%, 89.4%);
    --status-color: hsl(0, 0%, 60%);
    --supported-color: hsl(100, 100%, 30%);
    --non-standard-color: hsl(275.4, 77.7%, 35.1%);
    --in-development-color: hsl(24.5, 91.3%, 50.6%);
    --no-active-development-color: hsl(240, 60.6%, 59.2%);
    --partially-supported-color: hsl(180, 25%, 43.9%);
    --experimental-color: hsl(211.3, 100%, 50%);
    --under-consideration-color: hsl(5.9, 40.2%, 60%);
    --removed-not-considering-color: hsl(0, 0%, 49.8%);
    --not-implemented-color: hsl(0, 0%, 29.8%);
    --obsolete-color: hsl(50, 100%, 25.1%);
}

@media(prefers-color-scheme:dark) {
    :root {
        --feature-rule-color: hsl(0, 0%, 20%);
        --status-color: hsl(0, 0%, 51%);
        --supported-color: hsl(79.5, 45.3%, 52%);
        --non-standard-color: hsl(276.7, 36.3%, 51.4%);
        --in-development-color: hsl(24.5, 91.3%, 50.6%);
        --no-active-development-color: hsl(240, 60.6%, 59.2%);
        --partially-supported-color: hsl(180, 30%, 52%);
        --exoerimental-color: hsl(211.3, 100%, 50%);
        --under-consideration-color: hsl(0, 35%, 61%);
        --removed-not-considering-color: hsl(0, 0%, 49.8%);
        --not-implemented-color: hsl(0, 0%, 70.2%);
        --obsolete-color: hsl(31.9, 20.5%, 33.1%);
    }
}

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

.page h1 {
    font-size: 4.2rem;
    font-weight: 500;
    line-height: 6rem;
    margin: 3rem auto;
    width: 100%;
    text-align: center;
}

.page h1 a {
    color: inherit;
}

.page h2 {
    font-weight: 200;
    font-size: 3rem;
}

.page h3 {
    font-weight: 400;
    font-size: 2.2rem;
}

.css-feature-page {
    padding-bottom: 3rem;
}

.css-feature-page p {
    max-width: 920px;
    margin: 0 auto 3rem;
}

/* Feature Filters */
.feature-filters {
    background-color: hsl(0, 0%, 0%);
    background-color: var(--figure-mattewhite-background-color);
    width: 100vw;
    left: 50%;
    position: relative;
    transform: translate(-50vw, 0);
    box-sizing: border-box;
    margin-bottom: 3rem;
    border: 1px solid hsl(0, 0%, 90.6%);
    border-color: var(--article-border-color);
    border-left: none;
    border-right: none;
}

.feature-filters .search-input {
    background-repeat: no-repeat;
    background-position-x: 0.5rem;
    background-position-y: 1rem;
    background-size: 2rem;
    padding: 1rem;
    padding-left: 3rem;
    padding-right: 8.5rem;
    font-size: 2rem;
    width: 100%;
    margin-top: 0rem;
    margin-bottom: 0rem;
    box-sizing: border-box;
    border-color: transparent;
}

.feature-filters li {
    display: inline-block;
    white-space: no-wrap;
}

.property-status label,
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

#status-filters {
    display: none;
    text-align: center;
    margin-top: 1rem;
    margin-bottom: 0;
}

.property-filters {
    max-width: 920px;
    margin: 0 auto 0;
    position: relative;
    top: 0;
}

.property-filters.opened {
    margin-top: 1.5rem;
}

.property-filters.opened #status-filters {
    display: block;
}

#status-filters label {
    margin-left: 1rem;
    margin-bottom: 1rem;
    float: none;
    display: inline-block;
    position: relative;
}

.feature-filters label {
    float: none;
    display: inline-block;
}

.status-filters {
    list-style: none;
    display: inline-block;
    text-align: center;
}

.filter-toggle:checked + .filter-status {
    color: hsl(240, 1.3%, 84.5%);
    color: var(--text-color);
}

.filter-status,
.feature-status {
    color: hsl(0, 0%, 60%);
    color: var(--status-color);
    border-color: hsl(0, 0%, 60%);
    border-color: var(--status-color);
}

.feature-status a {
    color: inherit;
}

.status-filter,
.status-marker {
    border-color: hsl(0, 0%, 60%);
    border-color: var(--status-color)
}

.filter-toggle:checked + .filter-status {
    background-color: hsl(0, 0%, 60%);
    background-color: var(--status-color);
}

.property-description.status-marker {
    border-left-width: 3px;
    border-left-style: solid;
    padding: 0.5rem 0 0.5rem 1rem;
}

.property-count {
    max-width: 920px;
    margin: 0 auto 3rem;

    text-align: right;
    color: hsl(240, 2.3%, 56.7%);
    color: var(--text-color-coolgray);
}

.property-header > h3:first-of-type {
    -webkit-flex-grow: 1;
    flex-grow: 1;
    margin: 0;
}

.property-header:after {
    position: relative;
    width: 2rem;
    height: 2rem;
    right: 0;
    top: 0.5rem;
    margin-left: 1rem;
    transition: transform 0.3s ease-out;
}

.properties {
    padding: 0;
    max-width: 920px;
    margin: 0 auto 3rem;
    border-bottom: 1px solid hsl(0, 0%, 89.4%);
    border-color: var(--feature-rule-color);
}

.properties .property {
    position: relative;
    display: block;
    max-height: intrinsic;
    min-height: 3rem;
    overflow-y: hidden;
    cursor: pointer;
    background-color: transparent;
    border-color: transparent;
    border-width: 1px;
    border-style: solid;
    border-top-color: hsl(0, 0%, 89.4%);
    border-top-color: var(--feature-rule-color);
    padding: 0.5rem;
    line-height: 1.618;
    transition: background-color 0.3s ease-in;
}

.property.opened {
    background-color: hsl(0, 0%, 100%);
    background-color: var(--figure-mattewhite-background-color);
    border-left-color: hsl(0, 0%, 89.4%);
    border-left-color: var(--feature-rule-color);
    border-right-color: hsl(0, 0%, 89.4%);
    border-right-color: var(--feature-rule-color);
    max-height: 120rem;
}

.property.opened .property-header:after {
    -webkit-transform: rotateX(-180deg);
    -moz-transform: rotateX(-180deg);
    transform: rotateX(-180deg);
    perspective: 600;
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
    transform: rotateX(-180deg);
}

.property-header h3 .spec-label ,
.property-header h3 .spec-label a {
    text-decoration: none;
    font-weight: 200;
    color: hsl(0, 0%, 33.3%);
    color: var(--text-color-medium);
}

.spec-label::before {
    content: ' — ';
}

.property-description .toggleable {
    color: hsl(0, 0%, 20%);
    color: var(--text-color);
}

.property-header h3,
.property-header a[name] {
    color: hsl(0, 0%, 26.7%);
    color: var(--text-color-heading);
}

.property-alias {
    font-size: smaller;
}

.property-header p {
    margin-top: 0.5rem;
    margin-bottom: 0.5rem;
}

.property-alias,
.value-alias,
.value-status {
    color: hsl(240, 2.3%, 56.7%);
    color: var(--text-color-coolgray);
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


.property-status,
.property-status a {
    color: #999;
}

.property .status-marker {
    border-left-width: 3px;
    border-left-style: solid;
    padding: 0.5rem 0 0.5rem 1rem;
}


.status-marker {
    border-color: hsl(0, 0%, 60%);
    border-color: var(--status-color)
}

.supported {
    color: hsl(100, 100%, 30%);
    color: var(--supported-color);
    border-color: hsl(100, 100%, 30%);
    border-color: var(--supported-color);
}

.in-development {
    color: hsl(24.5, 91.3%, 50.6%);
    color: var(--in-development-color);
    border-color: hsl(24.5, 91.3%, 50.6%);
    border-color: var(--in-development-color);
}

.under-consideration {
    color: hsl(5.9, 40.2%, 60%);
    color: var(--under-consideration-color);
    border-color: hsl(5.9, 40.2%, 60%);
    border-color: var(--under-consideration-color);
}

.no-active-development {
    color: hsl(240, 60.6%, 59.2%);
    color: var(--no-active-development-color);
    border-color: hsl(240, 60.6%, 59.2%);
    border-color: var(--no-active-development-color);
}

.experimental {
    color: hsl(211.3, 100%, 50%);
    color: var(--experimental-color);
    border-color: hsl(211.3, 100%, 50%);
    border-color: var(--experimental-color);
}

.partial-support {
    color: hsl(180, 25%, 43.9%);
    color: var(--partially-supported-color);
    border-color: hsl(180, 25%, 43.9%);
    border-color: var(--partially-supported-color);
}

.non-standard {
    color: hsl(275.4, 77.7%, 35.1%);
    color: var(--non-standard-color);
    border-color: hsl(275.4, 77.7%, 35.1%);
    border-color: var(--non-standard-color);
}

.removed,
.not-considering {
    color: hsl(0, 0%, 49.8%);
    color: var(--removed-not-considering-color);
    border-color: hsl(0, 0%, 49.8%);
    border-color: var(--removed-not-considering-color);
}

.not-implemented {
    color: hsl(0, 0%, 29.8%);
    color: var(--not-implemented-color);
    border-color: hsl(0, 0%, 29.8%);
    border-color: var(--not-implemented-color);
}

.obsolete {
    color: hsl(50, 100%, 25.1%);
    color: var(--obsolete-color);
    border-color: hsl(50, 100%, 25.1%);
    border-color: var(--obsolete-color);
}

.by-specification {
    background-color: hsl(0, 0%, 96.9%);
    background-color: var(--content-background-color);
    border-color: hsl(0, 0%, 83.9%);
    border-color: var(--input-border-color);
}

.property-filters.opened .search-input {
    border-color: hsl(0, 0%, 83.9%);
    border-color: var(--input-border-color);
}

.feature-filters .filters-toggle-button {
    background-repeat: no-repeat;
    background-size: 2rem;
    background-position: right;
    background-filter: lightness(2);
    position: absolute;
    padding-right: 2.5rem;
    right: 1rem;
    top: 1rem;
    border: none;
    color: hsl(240, 2.3%, 56.7%);
}

.feature-filters .filters-toggle-button:hover {
    filter: brightness(0);
}

#filters-toggle {
    display: none;
}

.property-filters ul {
    margin-top: 0.5rem;
}

.property-filters ul li {
    margin-bottom: 0.5rem;
}

.property-filters label > input {
    position: relative;
    top: -1px;
}

.prefixes {
    display: none;
}

#specifications {
    display: inline-block;
    font-size: 1.6rem;
    color: hsl(0, 0%, 20%);
    color: var(--text-color);
    margin-left: 1rem;
}

.filter-by-specifications-toggle {
    position: absolute;
    display: none;
    left: 1000rem;
    top: 0;
    width: 100%;
    height: 100%;
}

#specifications:disabled + .filter-by-specifications-toggle {
    display: block;
    top: 0;
    left: 0;
}

h3 a[name], .admin-bar h3 a[name] {
    top: initial;
    width: auto;
    display: inline-block;
    visibility: visible; /* Override visibility:hidden from themes/webkit/style.css */
}

.pagination:after {
    display: none;
}

.pagination,
.pagination + h1 {
    margin-top: 0;
}

@media only screen and (max-width: 1180px) {
    .feature-filters .filters-toggle-button {
        right: 3rem;
    }
}

@media only screen and (max-width: 508px) {
    #property-filters,
    #property-list {
        width: 100%;
    }

    #property-filters {
        padding-left: 2rem;
        padding-right: 2rem;
    }

    .property-header h3 {
        font-size: 2rem;
        padding-right: 0.5rem;
    }

    .property-status {
        font-size: 1.6rem;
        margin-top: 0.4rem;
        float: left;
    }

    .property-header:after {
        width: 1rem;
        height: 1rem;
        background-size: 1rem;
        top: 1rem;
    }

    .property h3 {
        font-size: 2rem;
        padding-top: 4rem;
    }

    .property-header .property-status {
        font-size: 1.6rem;
        position: absolute;
        text-align: left;
    }

    .property .moreinfo {
        flex-wrap: wrap;
    }

    .property .moreinfo .contact {
        text-align: left;
    }

    .status-filters {
        flex-basis: 100%;
    }

    .status-filters label {
        margin-left: 0;
        margin-right: 1rem;
    }
}

@media(prefers-color-scheme:dark) {
    .property-header:after {
        filter: invert(1);
    }

    .search-input:hover,
    .search-input:focus,
    .feature-filters .filters-toggle-button:hover {
        filter: brightness(2);
    }
}

</style>
    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <div class="page css-feature-page" id="post-<?php the_ID(); ?>">

            <div class="connected pagination">
                <?php wp_nav_menu( array('theme_location'  => 'feature-subnav') ); ?>
            </div>

            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <section class="feature-filters">
                <form id="property-filters" class="property-filters page-width">
                    <input type="text" id="search" class="search-input" placeholder="Search CSS Features&hellip;" title="Filter the property list." required><label class="filters-toggle-button">Filters</label>
                    <ul id="status-filters">
                    </ul>

                    <div class="prefixes">
                        <h2>Filter by Prefix</h2>
                        <ul id="prefix-filters">
                        </ul>
                    </div>

                </form>
            </section>

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

        var descriptionContainer = document.createElement('div');
        descriptionContainer.className = "property-description status-marker";

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

        var statusContainer = document.createElement("div");
        descriptionContainer.classList.add(propertyObject.status.status);
        statusContainer.className = "property-status " + propertyObject.status.status;
        var statusLabel = document.createElement("label");

        if ("webkit-url" in propertyObject) {
            var statusLink = document.createElement("a");
            statusLink.href = propertyObject["webkit-url"];
            statusLink.textContent = readableStatus[propertyObject.status.status];
            statusLabel.appendChild(statusLink);
        } else {
            statusLabel.textContent = readableStatus[propertyObject.status.status];
        }
        statusContainer.appendChild(statusLabel);
        featureHeaderContainer.appendChild(statusContainer);

        var toggle = document.createElement('button');
        toggle.className = 'toggle';

        container.addEventListener('click', function (e) {
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
        var filtersToggleButton = document.getElementsByClassName('filters-toggle-button')[0];
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
            if (selectedStatuses.indexOf(canonicalStatus) != -1) {
                filtersForm.classList.add('opened');
                input.checked = true;
            }
            input.className = 'status-checkbox';
            input.addEventListener('change', function() { updateSearch(properties); });
            label.appendChild(input);
            label.className = "status-filter " + canonicalStatus;
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
            label.className = "status-filter by-specification";
            label.appendChild(document.createTextNode(" By Specification:"));

            var specsList = document.createElement('select');
            specsList.className = 'specifications';
            specsList.id = 'specifications';
            specsList.addEventListener('mousedown', function() {
                input.setAttribute('checked','checked');
                input.checked = true;
                console.log(input);
                input.checked = true;
            });
            var specsListToggle = document.createElement('div');
            specsListToggle.className = 'filter-by-specifications-toggle';
            label.appendChild(specsList);
            label.appendChild(specsListToggle);
            entry.appendChild(label);

            statusContainer.appendChild(entry);
        }

        filtersToggleButton.addEventListener('mousedown', function (e) {
            filtersForm.classList.toggle('opened');
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

        var prefixedValues = Array.from(prefixedPropertyObj.values);
        for (var valueObj of prefixedValues) {
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
        // Inherit "status" from the category if not explicitly specified.
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