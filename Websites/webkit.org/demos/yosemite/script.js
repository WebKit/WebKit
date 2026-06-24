const CATEGORIES = {
    viewpoint:      { label: "Viewpoint",       color: "#5E5CE6" },
    waterfall:      { label: "Waterfall",       color: "#007AFF" },
    campground:     { label: "Campground",      color: "#2EA44F" },
    trailhead:      { label: "Trailhead",       color: "#C68E1B" },
    visitor_center: { label: "Visitor Center",  color: "#6E6E73" },
};

const ORDER = ["viewpoint", "waterfall", "campground", "trailhead", "visitor_center"];

const POIS = [
    { category: "viewpoint",      placeId: "I7408F9590EC1AB75" },
    { category: "viewpoint",      placeId: "ICF2B4D5FA97B777D" },
    { category: "viewpoint",      placeId: "I3DDA071E9A782B52" },
    { category: "viewpoint",      placeId: "I48DA619DC25FE5B6" },
    { category: "waterfall",      placeId: "I51CD2AFB7B445AEA" },
    { category: "waterfall",      placeId: "IF9AFCCBAF0BA57A9" },
    { category: "waterfall",      placeId: "IF81C0FECEA0F7675" },
    { category: "waterfall",      placeId: "I4035417842960879" },
    { category: "campground",     placeId: "IE507EC4A256CD36D" },
    { category: "campground",     placeId: "IAC55606075DD1F06" },
    { category: "campground",     placeId: "I5422C46D418119A2" },
    { category: "trailhead",      placeId: "I5497A36255AF327A" },
    { category: "trailhead",      placeId: "IB7989AD49636870F" },
    { category: "trailhead",      placeId: "IABEB150CBD343ED0" },
    { category: "trailhead",      placeId: "I3C486610BDCA99E2" },
    { category: "visitor_center", placeId: "I61A354B7167CB5A8" },
    { category: "visitor_center", placeId: "IB5F50486480A2454" },
];

let map;
let annotations = [];
let isSatellite = false;

async function initMapKit() {
    await mapkit.load(["services", "map", "annotations"]);
    console.log("MapKit loads", mapkit.loadedLibraries);

    map = new mapkit.Map("map", {
        center: new mapkit.Coordinate(37.7456, -119.5936),
        cameraDistance: 10000,
        mapType: mapkit.MapType.Standard,
        showsCompass: mapkit.FeatureVisibility.Hidden,
        showsMapTypeControl: false,
        showsZoomControl: true,
    });

    // Fetch places and create annotations
    const placeLookup = new mapkit.PlaceLookup();

    annotations = await Promise.all(POIS.map(async (poi) => {
        const place = await placeLookup.getPlace(poi.placeId);
        const annotation = new mapkit.PlaceAnnotation(place, {
            selectionAccessory: new mapkit.PlaceSelectionAccessory()
        });
        annotation.data = { place, category: poi.category };
        return annotation;
    }));

    map.addAnnotations(annotations);
    renderList();

    map.addEventListener("select", (event) => {
        if (event.annotation && event.annotation.data) {
            document.querySelectorAll(".poi-item").forEach(el => {
                const selected = annotations[parseInt(el.dataset.index)] === event.annotation;
                el.classList.toggle("selected", selected);
                el.setAttribute("aria-pressed", selected ? "true" : "false");
            });
        }
    });

    // Map type toggle
    document.getElementById("toggle-map-type").addEventListener("click", () => {
        isSatellite = !isSatellite;
        map.mapType = isSatellite ? mapkit.MapType.Hybrid : mapkit.MapType.Standard;
        document.getElementById("toggle-map-type").textContent = isSatellite ? "Standard" : "Satellite";
    });
}

const headerTemplate = document.getElementById("tmpl-category-header");
const itemTemplate = document.getElementById("tmpl-poi-item");

function renderList() {
    const list = document.getElementById("poi-list");
    list.replaceChildren();
    document.getElementById("count").textContent = annotations.length + " places";

    ORDER.forEach(cat => {
        const items = annotations.filter(a => a.data.category === cat);
        if (!items.length) return;

        const { label, color } = CATEGORIES[cat];

        const header = headerTemplate.content.cloneNode(true).firstElementChild;
        const catLabel = header.querySelector(".cat-label");
        catLabel.style.color = color;
        catLabel.textContent = label + "s";
        list.appendChild(header);

        items.forEach(annotation => {
            const idx = annotations.indexOf(annotation);
            const item = itemTemplate.content.cloneNode(true).firstElementChild;
            item.dataset.index = idx;
            item.querySelector(".poi-item-name").textContent = annotation.title;
            item.setAttribute("aria-pressed", "false");
            item.addEventListener("click", () => {
                map.selectedAnnotation = annotation;
                map.setCenterAnimated(annotation.coordinate);
                document.querySelectorAll(".poi-item").forEach(el => {
                    el.setAttribute("aria-pressed", el === item ? "true" : "false");
                });
            });
            list.appendChild(item);
        });
    });
}

