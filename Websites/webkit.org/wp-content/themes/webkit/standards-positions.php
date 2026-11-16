<?php
/**
 * Template Name: Standards Positions
 **/
?>
<?php get_header(); ?>

<style>
#content { overflow-x:auto }
#content > div.page-width { width:100%; max-width:none; margin:auto }
#content h1 a { color:inherit }
#content p { width:80%; margin:0 auto }
#content p#filter-container { width:auto; font-size:2rem; margin-bottom:1em; padding:.2em; background:var(--figure-mattewhite-background-color); border: 1px solid var(--article-border-color); }
#content p#filter-container > input { width:80%; margin:0 auto; border:0; display:block; box-sizing:border-box; font-size:2rem; background-position-x:0.5rem; background-position-y:1rem; color:var(--text-color) }
#content p#filter-container > span { width:80%; margin:0 auto; display:block; text-align:right; font-size:1.5rem; }
#content p#filter-container > span input { margin-left:.5em; margin-right:1em }
#content p#filter-container > span select { margin-left:.5em; width:8em }
#content table { table-layout:fixed; border-collapse:collapse; width:80%; margin:0 auto 50px }
#content table th { font-weight:bold; padding:1px; text-align:left }
#content table th:nth-child(1) { width:2.5em }
#content table th:nth-child(3) { width:9.5em }
#content table th:nth-child(4) { width:5em }
#content table th:nth-child(5) { width:8em }
#content table th:nth-child(6) { width:5em }
#content table th:nth-child(7) { width:1.5em }
#content table td { padding:1px }
#content table td:nth-child(4) { text-align:center }
#content table td:nth-child(4) > span { display:block; border-radius:15px/20px }
#content table td.oppose > span { background:#e40303; color:#fff }
#content table td.support > span { background:#008026; color:#fff }
#content table td.neutral >span { background:#ffed00; color:#000 }
#content table > tbody > tr { background:none }
#content table > tbody > tr:nth-child(even of :not([hidden])) { background: #fff }
#content table > tbody > tr:target { background:yellow }

@media only screen and (max-width: 1200px) {
    #content table { width:90% }
}

@media only screen and (max-width: 1024px) {
    #content table th:nth-child(5),
    #content table td:nth-child(5) { display:none }
    #content table th:nth-child(6),
    #content table td:nth-child(6) { display:none }
}

@media only screen and (max-width: 800px) {
    #content table { width:98% }
    #content table th:nth-child(3),
    #content table td:nth-child(3) { display:none }
}

@media (prefers-color-scheme: dark) {
    #content table > tbody > tr:nth-child(even of :not([hidden])) { background:#000 }
    #content table > tbody > tr:target { background:darkred }
}
</style>

<h1><a href="./">Standards Positions</a></h1>

<p>Enable JavaScript for an interactive summary table of WebKit's standards positions. Failing that, browse the <a href="https://github.com/WebKit/standards-positions">standards-positions GitHub repository</a> directly.</p>

<script>
(async function () { // Begin closure

function createElement(name, attributes = {}, child = null) {
    const element = document.createElement(name);
    for (const [name, value] of Object.entries(attributes)) {
        element.setAttribute(name, value);
    }
    if (child !== null) {
        element.append(child);
    }
    return element;
}

function addElement(parent, name, attributes = {}, child = null) {
    return parent.appendChild(createElement(name, attributes, child));
}

function createStandardsPositionsTable(issues) {
    const fragment = new DocumentFragment();

    // Filters controls
    const p = addElement(fragment, "p", { id: "filter-container" });
    const input = addElement(p, "input", {
        class: "search-input",
        type: "search",
        id: "filter",
        placeholder: "Filter specifications…",
    });
    input.oninput = updateStandardsPositionsTableRows;
    const span = addElement(p, "span");
    const positionLabel = addElement(span, "label", {}, "Without position:");
    const positionInput = addElement(positionLabel, "input", {
        id: "filter-position",
        type: "checkbox",
        switch: "",
    });
    positionInput.oninput = updateStandardsPositionsTableRows;
    const topicLabel = addElement(span, "label", {}, "By topic:");
    const topicSelect = addElement(topicLabel, "select", {
        id: "filter-topic",
    });
    addTopicSelectOptions(topicSelect, issues);
    topicSelect.oninput = updateStandardsPositionsTableRows;

    // Table head
    const table = addElement(fragment, "table");
    const headerRow = addElement(addElement(table, "thead"), "tr");
    addElement(headerRow, "th", { scope: "col", title: "Discussion" }, "💬");
    for (const heading of [
        "Specification",
        "Concerns",
        "Position",
        "Topics",
        "Venues",
    ]) {
        addElement(headerRow, "th", { scope: "col" }, heading);
    }
    addElement(headerRow, "th", { scope: "col", title: "Self-link" }, "🔗");
    const tbody = addElement(table, "tbody");

    // Table body
    for (const issue of issues) {
        const id = issue["id"].substring(
            "https://github.com/WebKit/standards-positions/issues/".length
        );
        const hasPosition =
            issue["position"] !== null && issue["position"] !== "blocked";

        const row = addElement(tbody, "tr", { id: `position-${id}` });
        row.toggleAttribute("hidden", !hasPosition);
        addElement(
            row,
            "td",
            {},
            createElement("a", { href: issue["id"] }, `#${id}`)
        );
        if (issue["url"] !== null) {
            addElement(
                row,
                "td",
                {},
                createElement("a", { href: issue["url"] }, issue["title"])
            );
        } else {
            addElement(row, "td", {}, issue["title"]);
        }
        addElement(row, "td", {}, issue["concerns"].join(", "));
        if (hasPosition) {
            addElement(
                row,
                "td",
                { class: issue["position"] },
                createElement("span", {}, issue["position"])
            );
        } else if (issue["position"] === "blocked") {
            addElement(row, "td", {}, "review blocked");
        } else {
            addElement(row, "td");
        }
        addElement(row, "td", {}, issue["topics"].join(", "));
        addElement(row, "td", {}, issue["venues"].join(", "));
        if (hasPosition) {
            addElement(
                row,
                "td",
                {},
                createElement(
                    "a",
                    {
                        href: `#position-${id}`,
                        title: `Link to position: ${issue["title"]}`,
                    },
                    "⚓︎"
                )
            );
        } else {
            addElement(row, "td");
        }
    }
    return fragment;
}

function addTopicSelectOptions(parent, issues) {
    const topics = [];
    for (const issue of issues) {
        for (const topic of issue["topics"]) {
            if (!topics.includes(topic)) {
                topics.push(topic);
            }
        }
    }
    topics.sort();
    addElement(parent, "option", { value: "" }, "all");
    for (const topic of topics) {
        addElement(parent, "option", {}, topic);
    }
}

function updateStandardsPositionsTableRows() {
    const filterTextRE = createFilterTextRE(
        document.getElementById("filter").value
    );
    const filterTopic = document.getElementById("filter-topic").value;
    const filterPosition = !document.getElementById("filter-position").checked;
    for (const row of document.querySelector("main").querySelector("tbody")
        .rows) {
        row.toggleAttribute(
            "hidden",
            shouldRowBeHidden(row, filterTextRE, filterTopic, filterPosition)
        );
    }
}

function createFilterTextRE(filterText) {
    if (filterText) {
        // Replace once https://github.com/tc39/proposal-regex-escaping is a thing
        return new RegExp(
            filterText.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"),
            "i"
        );
    }
    return null;
}

function shouldRowBeHidden(row, filterTextRE, filterTopic, filterPosition) {
    hidden = false;
    if (filterTextRE && row.cells[1].textContent.search(filterTextRE) === -1) {
        hidden = true;
    } else if (
        filterTopic &&
        !row.cells[4].textContent
            .split(",")
            .map((item) => item.trim())
            .includes(filterTopic)
    ) {
        hidden = true;
    } else if (
        filterPosition &&
        (row.cells[3].className === "" || row.cells[3].className === "blocked")
    ) {
        hidden = true;
    }
    return hidden;
}

const container = document.querySelector("main");
const noScriptChild = container.querySelector("p");
const loadingChild = createElement(
    "p",
    {},
    "Please hold while we fetch data from GitHub."
);
noScriptChild.replaceWith(loadingChild);

try {
    const res = await fetch(
        "https://raw.githubusercontent.com/WebKit/standards-positions/main/summary.json"
    );
    const positions = await res.json();
    loadingChild.replaceWith(createStandardsPositionsTable(positions));
} catch (e) {
    loadingChild.replaceWith(
        createElement("p", {}, "Something went wrong. Please try again later.")
    );
    console.log(e);
}

})(); // End closure
</script>

<?php get_footer(); ?>
