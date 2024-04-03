import Chart from "chart.js/auto";
import { csvParse } from "d3-dsv";

// These 2 imports use some vite machinery to directly import these files as
// strings. Then they are constants and this reduces the load on the GC.
import airportsString from "./datasets/airports.csv?raw";
import flightsString from "./datasets/flights-airports.csv?raw";

/*
 * From https://www.omnicalculator.com/other/latitude-longitude-distance:
 * d = 2R × sin⁻¹(√[sin²((θ₂ - θ₁)/2) + cosθ₁ × cosθ₂ × sin²((φ₂ - φ₁)/2)])
 *
 * where:
 *     θ₁, φ₁ – First point latitude and longitude coordinates;
 *     θ₂, φ₂ – Second point latitude and longitude coordinates;
 *     R – Earth's radius (R = 6371 km); and
 *     d – Distance between them along Earth's surface.
 */
const R = 6371;
function computeDistance(coords1, coords2) {
    const long1 = toRadian(coords1.longitude);
    const lat1 = toRadian(coords1.latitude);
    const long2 = toRadian(coords2.longitude);
    const lat2 = toRadian(coords2.latitude);
    const longSquareSin = Math.sin((long2 - long1) / 2) ** 2;
    const latSquareSin = Math.sin((lat2 - lat1) / 2) ** 2;
    const d = 2 * R * Math.asin(Math.sqrt(latSquareSin + Math.cos(lat1) * Math.cos(lat2) * longSquareSin));
    return d;
}

const RAD = Math.PI / 180;
function toRadian(deg) {
    return deg * RAD;
}

let preparedData = null;
function prepare() {
    /**
     * AirportInformation: { state, iata, name, city, country, latitude, longitude }
     * airports: Array<AirportInformation>
     * flights: Array<{ origin, destination, count }>
     */
    const airports = csvParse(airportsString);
    const flights = csvParse(flightsString);
    const airportMap = new Map(airports.map((airport) => [airport.iata, airport]));

    for (const flight of flights) {
        const origAirport = airportMap.get(flight.origin);
        const destAirport = airportMap.get(flight.destination);
        flight.distance = computeDistance(origAirport, destAirport);
    }

    preparedData = { flights, airportMap };
}

const ROOT = document.getElementById("chart");
const opaqueCheckBox = document.getElementById("opaque-color");

let currentChart = null;
function drawScattered(data) {
    if (!preparedData)
        throw new Error("Please prepare the data first.");

    reset();

    currentChart = new Chart(ROOT, {
        type: "scatter",
        data: {
            datasets: [
                {
                    data: preparedData.flights,
                    backgroundColor: opaqueCheckBox.checked ? "rgb(0, 125, 255)" : "rgba(0, 125, 255, .20)",
                },
            ],
        },
        options: {
            animation: false,
            parsing: {
                xAxisKey: "distance",
                yAxisKey: "count",
            },
            plugins: {
                legend: {
                    display: false,
                },
                title: {
                    display: true,
                    text: "Number of flights for a distance",
                    position: "bottom",
                },
                tooltip: {
                    displayColors: false,
                    callbacks: {
                        label: (item) => {
                            const orig = preparedData.airportMap.get(item.raw.origin);
                            const dest = preparedData.airportMap.get(item.raw.destination);
                            const result = `✈ ${orig.name} (${orig.iata}) → ${dest.name} (${dest.iata}): ${Math.round(item.raw.distance)} km`;
                            return result;
                        },
                    },
                },
            },
            scales: {
                x: {
                    title: {
                        text: "distance →",
                        align: "end",
                        display: true,
                    },
                    ticks: {
                        format: {
                            style: "unit",
                            unit: "kilometer",
                        },
                    },
                },
                y: {
                    type: "logarithmic",
                    title: {
                        text: "# of flights →",
                        align: "end",
                        display: true,
                    },
                },
            },
        },
    });
}

function openTooltip() {
    if (!currentChart)
        throw new Error("No chart is present, please draw a chart first");

    const renderedDataset = currentChart.getDatasetMeta(0);
    const node = currentChart.canvas;
    const rect = node.getBoundingClientRect();

    // Index 2426 is carefully chosen to display a lot of lines (which depends
    // on the zoom level).
    const point = renderedDataset.data[2426];
    const event = new MouseEvent("mousemove", {
        clientX: rect.left + point.x,
        clientY: rect.top + point.y,
        cancelable: true,
        bubbles: true,
    });

    node.dispatchEvent(event);
}

function reset() {
    if (currentChart) {
        currentChart.destroy();
        currentChart = null;
    }
}

async function runAllTheThings() {
    [
        // Force prettier to use a multiline formatting
        "prepare",
        "add-scatter-chart-button",
        "open-tooltip",
    ].forEach((id) => document.getElementById(id).click());
}

document.getElementById("prepare").addEventListener("click", prepare);
document.getElementById("add-scatter-chart-button").addEventListener("click", drawScattered);
document.getElementById("open-tooltip").addEventListener("click", openTooltip);
document.getElementById("reset").addEventListener("click", reset);
document.getElementById("run-all").addEventListener("click", runAllTheThings);

if (import.meta.env.DEV)
    runAllTheThings();
