import { Suites, Tags } from "./tests.mjs";
import { params, defaultParams } from "./params.mjs";

export function createDeveloperModeContainer() {
    const container = document.createElement("div");
    container.className = "developer-mode";

    const details = document.createElement("details");
    const summary = document.createElement("summary");
    summary.textContent = "Developer Mode";
    details.append(summary);

    const content = document.createElement("div");
    content.className = "developer-mode-content";

    content.append(createUIForSuites());

    const settings = document.createElement("div");
    settings.className = "settings";
    settings.append(createUIForIterationCount());
    settings.append(createUIForMeasurementMethod());
    settings.append(createUIForWarmupSuite());
    settings.append(createUIForWarmupBeforeSync());
    settings.append(createUIForSyncStepDelay());

    content.append(document.createElement("hr"));
    content.append(settings);

    content.append(document.createElement("hr"));
    content.append(createUIForRun());

    details.append(content);
    container.append(details);
    return container;
}

function span(text) {
    const span = document.createElement("span");
    span.textContent = text;
    return span;
}

function createUIForMeasurementMethod() {
    let check = document.createElement("input");
    check.type = "checkbox";
    check.id = "measurement-method";
    check.checked = params.measurementMethod === "raf";

    check.onchange = () => {
        params.measurementMethod = check.checked ? "raf" : "timer";
        updateURL();
    };

    let label = document.createElement("label");
    label.append(check, " ", span("rAF timing"));

    return label;
}

function createUIForWarmupSuite() {
    let check = document.createElement("input");
    check.type = "checkbox";
    check.id = "warmup-suite";
    check.checked = !!params.useWarmupSuite;

    check.onchange = () => {
        params.useWarmupSuite = check.checked;
        updateURL();
    };

    let label = document.createElement("label");
    label.append(check, " ", span("Use Warmup Suite"));

    return label;
}

function createUIForIterationCount() {
    const { range, label } = createTimeRangeUI("Iterations: ", params.iterationCount, "#", 1, 200);
    range.onchange = () => {
        params.iterationCount = parseInt(range.value);
        updateURL();
    };
    return label;
}

function createUIForWarmupBeforeSync() {
    const { range, label } = createTimeRangeUI("Warmup time: ", params.warmupBeforeSync);
    range.onchange = () => {
        params.warmupBeforeSync = parseInt(range.value);
        updateURL();
    };
    return label;
}

function createUIForSyncStepDelay() {
    const { range, label } = createTimeRangeUI("Sync step delay: ", params.waitBeforeSync);
    range.onchange = () => {
        params.waitBeforeSync = parseInt(range.value);
        updateURL();
    };
    return label;
}

function createTimeRangeUI(labelText, initialValue, unit = "ms", min = 0, max = 1000) {
    const range = document.createElement("input");
    range.type = "range";
    range.min = min;
    range.max = max;
    range.value = initialValue;

    const rangeValueAndUnit = document.createElement("span");
    rangeValueAndUnit.className = "range-label-data";

    const rangeValue = document.createElement("span");
    rangeValue.textContent = initialValue;
    rangeValueAndUnit.append(rangeValue, " ", unit);

    const label = document.createElement("label");
    label.append(span(labelText), range, rangeValueAndUnit);

    range.oninput = () => {
        rangeValue.textContent = range.value;
    };

    return { range, label };
}

function createUIForSuites() {
    const control = document.createElement("nav");
    control.className = "suites";
    const ol = document.createElement("ol");
    const checkboxes = [];
    const setSuiteEnabled = (suiteIndex, enabled) => {
        Suites[suiteIndex].disabled = !enabled;
        checkboxes[suiteIndex].checked = enabled;
    };

    for (const suite of Suites) {
        const li = document.createElement("li");
        const checkbox = document.createElement("input");
        checkbox.id = suite.name;
        checkbox.type = "checkbox";
        checkbox.checked = !suite.disabled;
        checkbox.onchange = () => {
            suite.disabled = !checkbox.checked;
            updateURL();
        };
        checkboxes.push(checkbox);

        const label = document.createElement("label");
        label.append(checkbox, " ", suite.name);
        li.appendChild(label);
        label.onclick = (event) => {
            if (event?.ctrlKey || event?.metaKey) {
                for (let suiteIndex = 0; suiteIndex < Suites.length; suiteIndex++) {
                    if (Suites[suiteIndex] !== suite)
                        setSuiteEnabled(suiteIndex, false);
                    else
                        setSuiteEnabled(suiteIndex, true);
                }
            }
        };

        ol.appendChild(li);
    }
    control.appendChild(ol);
    let buttons = control.appendChild(document.createElement("div"));
    buttons.className = "button-bar";

    let button = document.createElement("button");
    button.textContent = "Select all";
    button.onclick = () => {
        for (let suiteIndex = 0; suiteIndex < Suites.length; suiteIndex++)
            setSuiteEnabled(suiteIndex, true);

        updateURL();
    };
    buttons.appendChild(button);

    button = document.createElement("button");
    button.textContent = "Unselect all";
    button.onclick = () => {
        for (let suiteIndex = 0; suiteIndex < Suites.length; suiteIndex++)
            setSuiteEnabled(suiteIndex, false);

        updateURL();
    };
    buttons.appendChild(button);

    let i = 0;
    const kTagsPerLine = 3;
    for (const tag of Tags) {
        if (tag === "all")
            continue;
        if (!(i % kTagsPerLine)) {
            buttons = control.appendChild(document.createElement("div"));
            buttons.className = "button-bar";
        }
        i++;
        button = document.createElement("button");
        button.className = "tag";
        button.textContent = `#${tag}`;
        button.dataTag = tag;
        button.onclick = (event) => {
            const extendSelection = event?.shiftKey;
            const invertSelection = event?.ctrlKey || event?.metaKey;
            const selectedTag = event.target.dataTag;
            for (let suiteIndex = 0; suiteIndex < Suites.length; suiteIndex++) {
                let enabled = Suites[suiteIndex].tags.includes(selectedTag);
                if (invertSelection)
                    enabled = !enabled;
                if (extendSelection && !enabled)
                    continue;
                setSuiteEnabled(suiteIndex, enabled);
            }
            updateURL();
        };
        buttons.appendChild(button);
    }

    return control;
}

function createUIForRun() {
    let button = document.createElement("button");
    button.textContent = "Start Test";
    button.onclick = (event) => {
        globalThis.benchmarkClient.start();
    };
    let buttons = document.createElement("div");
    buttons.className = "button-bar";
    buttons.appendChild(button);
    return buttons;
}

function updateURL() {
    const url = new URL(window.location.href);

    // If less than all suites are selected then change the URL "Suites" GET parameter
    // to comma separate only the selected
    const selectedSuites = Suites.filter((suite) => !suite.disabled);

    if (!selectedSuites.length) {
        url.searchParams.delete("tags");
        url.searchParams.delete("suites");
        url.searchParams.delete("suite");
    } else {
        url.searchParams.delete("tags");
        url.searchParams.delete("suite");
        // Try finding common tags that would result in the current suite selection.
        let commonTags = new Set(selectedSuites[0].tags);
        for (const suite of Suites) {
            if (suite.disabled)
                suite.tags.forEach((tag) => commonTags.delete(tag));
            else
                commonTags = new Set(suite.tags.filter((tag) => commonTags.has(tag)));
        }
        if (commonTags.size) {
            const tags = [...commonTags][0];
            if (tags === "default")
                url.searchParams.delete("tags");
            else
                url.searchParams.set("tags", tags);
            url.searchParams.delete("suites");
        } else {
            url.searchParams.delete("tags");
            url.searchParams.set("suites", selectedSuites.map((suite) => suite.name).join(","));
        }
    }

    if (params.measurementMethod !== "raf")
        url.searchParams.set("measurementMethod", "timer");
    else
        url.searchParams.delete("measurementMethod");

    const boolParamKeys = ["iterationCount", "useWarmupSuite", "warmupBeforeSync", "waitBeforeSync"];
    for (const paramKey of boolParamKeys) {
        if (params[paramKey] !== defaultParams[paramKey])
            url.searchParams.set(paramKey, params[paramKey]);
        else
            url.searchParams.delete(paramKey);
    }

    // Only push state if changed
    url.search = decodeURIComponent(url.search);
    if (url.href !== window.location.href)
        window.history.pushState({}, "", url);
}
