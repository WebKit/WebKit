// FontCascade::fallbackRangesAt hands out a reference into
// FontCascadeFonts::m_realizedFallbackRanges. Resolving a character walks those ranges and
// resolves fonts with ExternalResourceDownloadPolicy::Allow, which on a worker completes the
// load synchronously and reaches FontCascadeCache::pruneUnreferencedEntries, destroying the
// FontCascadeFonts being walked.

function paddedFontDataURL(path, paddingLength)
{
    const fontRequest = new XMLHttpRequest();
    fontRequest.open('GET', path, false);
    fontRequest.responseType = 'arraybuffer';
    fontRequest.send();
    // Padding keeps the data URL over the 1 MB limit in
    // CSSFontFaceSource::opportunisticallyStartFontDataURLLoading, so the face stays Pending
    // until the walk below pumps it.
    const padding = new Uint8Array(paddingLength);
    const blob = new Blob([fontRequest.response, padding], { type: 'font/woff' });
    return new FileReaderSync().readAsDataURL(blob);
}

const dataURL = paddedFontDataURL('../../../../resources/Ahem.woff', 800000);

// One family with several distinct requests, so the walk pumps several times and each pump
// runs FontCascade::update and advances FontCascadeCache's prune counter.
const faceCount = 4;
const faces = [];
for (let faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
    const face = new FontFace('paddedAhem', 'url("' + dataURL + '#' + faceIndex + '")');
    faces.push(face);
    self.fonts.add(face);
}

// Bring the prune counter to one below its interval so that a pump during the walk crosses it.
const primingContext = new OffscreenCanvas(1, 1).getContext('2d');
for (let primeIndex = 0; primeIndex < 1000 - faceCount; ++primeIndex)
    primingContext.font = '11px primingFamily' + primeIndex;

// Times has no glyph for U+13A0, so resolving it walks down to paddedAhem.
const context = new OffscreenCanvas(400, 400).getContext('2d');
context.font = '32px Times, paddedAhem';
context.fillText(String.fromCharCode(0x13A0) + String.fromCharCode(0x0301), 40, 200);

postMessage(faces.map(face => face.status).join(','));
