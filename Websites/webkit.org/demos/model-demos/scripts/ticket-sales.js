const $ = (id) => document.getElementById(id);

const model              = $('model');
const seatMap            = $('seatMap');
const seatPreview        = $('seatPreview');
const previewBackdrop    = $('previewBackdrop');
const previewSpinner     = $('previewSpinner');
const previewUnsupported = $('previewUnsupported');

// Hardcoded "in cart" seats for the demo
const CART_SEATS = new Set(['BD44', 'BD45']);

let modelReady     = false;
let popupOpen      = false;
let pendingReenter = false;
let currentSeat    = null;
let activeSeatId   = null;
let seatMapSvg     = null;

// Immersive model API used in this demo:
//   - model.requestImmersive(), document.exitImmersive()
//   - document.immersiveEnabled, document.immersiveElement
//   - immersivechange event
//   - model.entityTransform   (different transform inline vs immersive)
//   - model.ready             (async load promise)

function buildSceneTransform(isImmersive, seat) {
    const t = new DOMMatrix();
    if (isImmersive) {
        t.rotateSelf(0, 45, 0);
    } else {
        t.scaleSelf(0.1, 0.1, 0.1);
        t.translateSelf(0, -1.1, 0);
    }
    t.rotateSelf(0, -seat.pos.rotation_y, 0);
    t.translateSelf(-seat.pos.x, -seat.pos.y, -seat.pos.z);
    return t;
}

function applySceneTransform() {
    if (!modelReady || !currentSeat) return;
    model.entityTransform = buildSceneTransform(!!document.immersiveElement, currentSeat);
}

model.addEventListener('immersivechange', async () => {
    const isImmersive = !!document.immersiveElement;

    // Switching seats while immersive: re-enter at the new pose
    if (!isImmersive && pendingReenter) {
        pendingReenter = false;
        if (currentSeat)
            model.entityTransform = buildSceneTransform(true, currentSeat);
        try {
            await model.requestImmersive();
        } catch (err) {
            console.error('Re-enter failed:', err);
            seatPreview.classList.remove('immersive');
            updateImmersiveBtn(false);
            applySceneTransform();
            updateFloatingExitBtn();
        }
        return;
    }

    seatPreview.classList.toggle('immersive', isImmersive);
    updateImmersiveBtn(isImmersive);
    applySceneTransform();
    updateFloatingExitBtn();
    if (!isImmersive && !popupOpen) {
        setActiveSeat(null);
        currentSeat = null;
    }
});

function updateImmersiveBtn(isImmersive) {
    $('immersiveBtn').querySelector('span').textContent = isImmersive ? 'Exit Immersive' : 'Immersive Preview';
}

$('immersiveBtn').addEventListener('click', async () => {
    if (seatPreview.classList.contains('immersive')) {
        document.exitImmersive();
    } else {
        seatPreview.classList.add('immersive');
        updateImmersiveBtn(true);
        try { await model.requestImmersive(); }
        catch { seatPreview.classList.remove('immersive'); updateImmersiveBtn(false); }
    }
});
$('seatmapExitBtn').addEventListener('click', () => document.exitImmersive());

if (model.ready) {
    model.ready
        .then(() => {
            modelReady = true;
            if (document.immersiveEnabled)
                $('immersiveBtn').hidden = false;
            applySceneTransform();
        })
        .catch(() => { previewUnsupported.hidden = false; })
        .finally(() => { previewSpinner.hidden = true; });
} else {
    previewSpinner.hidden = true;
    previewUnsupported.hidden = false;
}

// Popup

function togglePopup(show) {
    popupOpen = show;
    previewBackdrop.classList.toggle('visible', show);
    if (!show && !document.immersiveElement) {
        setActiveSeat(null);
        currentSeat = null;
    }
}

function updateFloatingExitBtn() {
    $('seatmapExitBtn').hidden = !document.immersiveElement;
}

previewBackdrop  .addEventListener('click', (e) => { if (e.target === previewBackdrop) togglePopup(false); });
$('dismissBtn')  .addEventListener('click', () => togglePopup(false));
$('addToCartBtn').addEventListener('click', () => togglePopup(false));

// Seat selection

function renderPreviewInfo(seat) {
    $('seatName')    .textContent = seat.tier;
    $('seatDetail')  .textContent = `Row ${seat.row}, Seat ${seat.number}`;
    $('seatPrice')   .textContent = `$${seat.price}`;
    $('addToCartBtn').textContent = CART_SEATS.has(seat.id) ? 'Remove from Cart' : 'Add to Cart';
}

function setActiveSeat(id) {
    if (activeSeatId)
        seatMapSvg?.querySelector(`[data-seat-id="${activeSeatId}"]`)?.classList.remove('active');
    activeSeatId = id;
    if (id)
        seatMapSvg?.querySelector(`[data-seat-id="${id}"]`)?.classList.add('active');
}

async function selectSeat(seat) {
    const isNewSeat = currentSeat?.id !== seat.id;
    currentSeat = seat;
    setActiveSeat(seat.id);
    renderPreviewInfo(seat);
    togglePopup(true);

    if (!isNewSeat) return;

    if (document.immersiveElement) {
        pendingReenter = true;
        await document.exitImmersive();
    } else {
        applySceneTransform();
    }
}

// Seat map

async function loadSeatMap() {
    const [data, svgText] = await Promise.all([
        fetch('data/theater-availability.json').then((r) => r.json()),
        fetch('data/theater-seatmap.svg').then((r) => r.text()),
    ]);

    $('eventSeats').textContent = `${data.event.available_count} seats available`;
    $('eventPrice').textContent = `$${data.event.price_min} – $${data.event.price_max}`;

    const seatLookup = new Map(data.seats.map((s) => [s.id, s]));

    function selectSeatFromCircle(circle) {
        const seatButton = circle.dataset;
        const seatData = seatLookup.get(seatButton.seatId);
        if (!seatData) return;
        selectSeat({
            id:     seatButton.seatId,
            tier:   seatButton.section,
            row:    seatButton.row,
            number: parseInt(seatButton.seatNumber, 10),
            price:  seatData.price,
            pos:    seatData.transform,
        });
    }

    seatMap.innerHTML = '';
    const viewport = document.createElement('div');
    viewport.className = 'seatmap-viewport';
    const layer = document.createElement('div');
    layer.className = 'seatmap-layer';
    layer.innerHTML = svgText;
    viewport.appendChild(layer);
    seatMap.appendChild(viewport);

    seatMapSvg = layer.querySelector('svg');
    const svgW = parseFloat(seatMapSvg.getAttribute('width'));
    const svgH = parseFloat(seatMapSvg.getAttribute('height'));

    for (const circle of seatMapSvg.querySelectorAll('.seat'))
        if (!seatLookup.has(circle.dataset.seatId)) circle.classList.add('unavailable');

    for (const id of CART_SEATS)
        seatMapSvg.querySelector(`[data-seat-id="${id}"]`)?.classList.add('in-cart');

    const availableSeats = [...seatMapSvg.querySelectorAll('.seat:not(.unavailable)')];
    const RENDER_SEAT_PX = 24;
    const ZOOM_SEAT_PX   = 32;
    const seatRadiusSvg  = availableSeats[0]?.r?.baseVal?.value || 10;
    const renderScale    = RENDER_SEAT_PX / (2 * seatRadiusSvg);
    const renderedW      = svgW * renderScale;
    const renderedH      = svgH * renderScale;
    const zoomedScale    = ZOOM_SEAT_PX / RENDER_SEAT_PX;
    seatMapSvg.style.width  = `${renderedW}px`;
    seatMapSvg.style.height = `${renderedH}px`;

    const resetZoomBtn = document.createElement('button');
    resetZoomBtn.className = 'pill-btn seatmap-fit-btn';
    resetZoomBtn.type = 'button';
    resetZoomBtn.hidden = true;
    resetZoomBtn.setAttribute('aria-label', 'Reset zoom');
    resetZoomBtn.innerHTML = '<img src="images/svg/reset-zoom.svg" alt="" width="14" height="14"><span>Reset zoom</span>';
    resetZoomBtn.addEventListener('click', (e) => { e.stopPropagation(); fitMap(); });
    viewport.appendChild(resetZoomBtn);

    let scale = 1, panX = 0, panY = 0;
    let isZoomed = false;
    const apply = () => { layer.style.transform = `translate(${panX}px, ${panY}px) scale(${scale})`; };

    function fitMap() {
        const r = viewport.getBoundingClientRect();
        if (!r.width || !r.height) return;
        scale = Math.min(r.width / renderedW, r.height / renderedH);
        panX = (r.width  - renderedW * scale) / 2;
        panY = (r.height - renderedH * scale) / 2;
        isZoomed = false;
        resetZoomBtn.hidden = true;
        apply();
    }

    function zoomMapToPoint(viewX, viewY) {
        const layerX = (viewX - panX) / scale;
        const layerY = (viewY - panY) / scale;
        scale = zoomedScale;
        panX = viewX - layerX * scale;
        panY = viewY - layerY * scale;
        isZoomed = true;
        resetZoomBtn.hidden = false;
        apply();
    }

    viewport.addEventListener('click', (e) => {
        if (!isZoomed) {
            const r = viewport.getBoundingClientRect();
            zoomMapToPoint(e.clientX - r.left, e.clientY - r.top);
            return;
        }
        const seat = e.target.closest('.seat:not(.unavailable)');
        if (seat) selectSeatFromCircle(seat);
        else fitMap();
    });

    fitMap();
    requestAnimationFrame(() => layer.classList.add('animated'));
    new ResizeObserver(fitMap).observe(viewport);
}

loadSeatMap().catch((err) => {
    console.error('Failed to load seat map:', err);
    seatMap.innerHTML = '<div class="seat-map-loading">Failed to load seat map</div>';
});
