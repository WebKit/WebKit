import * as Plot from "@observablehq/plot";
import { csvParse } from "d3-dsv";
import * as d3Array from "d3-array";
import { format as d3Format } from "d3-format";

// These 2 imports use some vite machinery to directly import these files as
// strings. Then they are constants and this reduces the load on the GC.
import airportsString from "./datasets/airports.csv?raw";
import flightsString from "./datasets/flights-airports.csv?raw";

const ROOT = document.getElementById("chart");
const DEFAULT_OPTIONS = {
    // This width is really a max-width: the plot adjusts using the available
    // width as well.
    width: 2000,
    // The height is especially used for the ratio.
    height: 1000,
};

let preparedData;
function prepare() {
    /**
     * AirportInformation: { state, iata, name, city, country, latitude, longitude }
     * airports: Array<AirportInformation>
     * flights: Array<{ origin, destination, count }>
     */
    const { airports, flights } = parseAirportsInformation();
    /**
     * flightsByAirport: Map<iata: string, { origin: number, destination: number, total: number}>
     */
    const flightsByAirport = groupFlightsByAirports(flights);
    /**
     * byAirport: Map<iata: string, AirportInformation>
     */
    const byAirport = d3Array.index(airports, (d) => d.iata);

    /* Array<[state, AirportInformation[]]> */
    const airportsGroupedByStateArray = d3Array.groups(airports, (d) => d.state);

    /* DescSortedArray<[{ state: string, total: number, mostUsedAirportsInState: AirportInformation[] }]> */
    const stateInformationSortedArray = airportsGroupedByStateArray
        .map(([state, airportsInState]) => {
            const totalFlightsInState = d3Array.sum(airportsInState, ({ iata }) => flightsByAirport.get(iata)?.total);
            const sorted = d3Array.sort(airportsInState, ({ iata }) => -flightsByAirport.get(iata)?.total);
            const mostUsedAirportsInState = sorted.slice(0, airportCountPerGroup());
            return {
                state,
                total: totalFlightsInState,
                mostUsedAirports: mostUsedAirportsInState,
            };
        })
        .sort((stateA, stateB) => stateB.total - stateA.total);

    /* Array<state: string> */
    const stateSortedArray = stateInformationSortedArray.map(({ state }) => state);

    // Flatten the information in preparedData.stateInformationSortedArray, so that we
    // have one item == one airport information.
    /* Array<{state, iata, name, city, index, origin, destination, total}> */
    const plotData = stateInformationSortedArray.flatMap(({ mostUsedAirports, total, state }) => {
        const enrichedMostUsedAirports = mostUsedAirports.map(({ iata, name, city }, index) => ({
            state,
            iata,
            name,
            city,
            index, // This will be used to have consistent colors.
            ...flightsByAirport.get(iata),
        }));
        const otherTotal = total - d3Array.sum(mostUsedAirports, ({ iata }) => flightsByAirport.get(iata)?.total);

        if (otherTotal > 0) {
            enrichedMostUsedAirports.push({
                state,
                iata: "Other",
                total: otherTotal,
                index: enrichedMostUsedAirports.length,
            });
        }
        return enrichedMostUsedAirports;
    });

    preparedData = {
        airports,
        flights,
        flightsByAirport,
        byAirport,
        stateInformationSortedArray,
        stateSortedArray,
        plotData,
    };
}

function parseAirportsInformation() {
    return {
        airports: csvParse(airportsString),
        flights: csvParse(flightsString),
    };
}

function groupFlightsByAirports(flights) {
    const flightsByAirport = new Map();

    for (const { origin, destination, count } of flights) {
        const infoForOriginAirport = flightsByAirport.get(origin) ?? { origin: 0, destination: 0, total: 0 };
        const intCount = Number(count);
        if (Number.isNaN(intCount)) {
            console.error(`Couldn't convert ${count} to number.`);
            continue;
        }
        infoForOriginAirport.origin += intCount;
        infoForOriginAirport.total += intCount;
        flightsByAirport.set(origin, infoForOriginAirport);
        const infoForDestinationAirport = flightsByAirport.get(destination) ?? { origin: 0, destination: 0, total: 0 };
        infoForDestinationAirport.destination += intCount;
        infoForDestinationAirport.total += intCount;
        flightsByAirport.set(destination, infoForDestinationAirport);
    }

    return flightsByAirport;
}

function isReady() {
    return !!preparedData;
}

function addStackedBars() {
    if (!isReady())
        throw new Error("Please prepare the data first.");

    const options = {
        ...DEFAULT_OPTIONS,
        color: { type: "categorical" },
        x: {
            domain: preparedData.stateSortedArray,
        },
        y: { grid: true, tickFormat: "~s" },
        marks: [
            Plot.barY(preparedData.plotData, {
                x: "state",
                y: "total",
                fill: "index",
                title: (d) => `${d.iata === "Other" ? "Other" : `${d.name}, ${d.city} (${d.iata})`}\n${d3Format(",")(d.total)} flights`,
            }),
            Plot.text(preparedData.stateInformationSortedArray, { x: "state", y: "total", text: (d) => d3Format(".2~s")(d.total), dy: -10 }),
            Plot.ruleY([0]),
        ],
    };
    ROOT.append(Plot.plot(options));
}

function addDottedBars() {
    if (!isReady())
        throw new Error("Please prepare the data first.");

    const data = [...preparedData.flightsByAirport]
        .flatMap(([iata, { origin, destination }]) => {
            const airportInformation = preparedData.byAirport.get(iata);
            return [
                { ...airportInformation, value: -origin },
                { ...airportInformation, value: destination },
            ];
        })
        .filter((d) => d.value);

    const options = {
        ...DEFAULT_OPTIONS,
        color: { type: "threshold", domain: [0] },
        x: {
            domain: preparedData.stateSortedArray,
        },
        y: {
            grid: true,
            label: "← outward          Number of flights          inward →",
            labelAnchor: "center",
            tickFormat: (v) => d3Format("~s")(Math.abs(v)),
            type: "pow",
            exponent: 0.2,
        },
        marks: [
            Plot.dot(data, {
                x: "state",
                y: "value",
                r: 4,
                stroke: "value",
                strokeWidth: 3,
                title: (d) => `${d.iata === "Other" ? "Other" : `${d.name}, ${d.city} (${d.iata})`}\n${d3Format(",")(Math.abs(d.value))} ${d.value > 0 ? "inward" : "outward"} flights`,
            }),
            Plot.ruleY([0]),
        ],
    };

    ROOT.append(Plot.plot(options));
}

function reset() {
    ROOT.textContent = "";
}

async function runAllTheThings() {
    reset();
    [
        // Force prettier to use a multiline formatting
        "prepare",
        "add-stacked-chart-button",
        "add-dotted-chart-button",
    ].forEach((id) => document.getElementById(id).click());
}

// This is the number of airports we keep for each state in the stacked bar
// graph. One additional group will be added, that will sum all airports that
// haven't been kept. It's retrieved from the input directly.
function airportCountPerGroup() {
    return document.querySelector("#airport-group-size-input").value;
}

function onGroupSizeInputChange() {
    document.querySelector("#airport-group-size").textContent = airportCountPerGroup();
    if (import.meta.env.DEV) {
        // In dev mode, redraw everything
        runAllTheThings();
    }
}

document.getElementById("prepare").addEventListener("click", prepare);
document.getElementById("add-stacked-chart-button").addEventListener("click", addStackedBars);
document.getElementById("add-dotted-chart-button").addEventListener("click", addDottedBars);
document.getElementById("reset").addEventListener("click", reset);
document.getElementById("run-all").addEventListener("click", runAllTheThings);
document.getElementById("airport-group-size-input").addEventListener("input", onGroupSizeInputChange);
onGroupSizeInputChange();

if (import.meta.env.DEV)
    runAllTheThings();
