// In a worker, loading a small data: URL font completes synchronously, so the
// pre-warm loop in CSSFontSelector::opportunisticallyStartFontDataURLLoading
// resolves FontFace.loaded and runs author script while it is still iterating.
// That script calls FontFaceSet.add(), which clears CSSFontFaceSet::m_cache.

function fontDataURL(path)
{
    const fontRequest = new XMLHttpRequest();
    fontRequest.open('GET', path, false);
    fontRequest.responseType = 'arraybuffer';
    fontRequest.send();
    return new FileReaderSync().readAsDataURL(new Blob([fontRequest.response], { type: 'font/woff' }));
}

const dataUrl = fontDataURL('../../../../resources/Ahem.woff');

const face1 = new FontFace('testFace', 'url("' + dataUrl + '")');
const face2 = new FontFace('testFace', 'url("' + dataUrl + '")');
self.fonts.add(face1);
self.fonts.add(face2);

// Populating the deferred promise makes resolution observable below.
void face1.loaded;
void face2.loaded;

const reenterFontFaceSet = function () {
    self.fonts.add(new FontFace('unrelated', 'local("nonexistent")'));
};
Object.defineProperty(face1, 'then', { get: reenterFontFaceSet });
Object.defineProperty(face2, 'then', { get: reenterFontFaceSet });

const context = new OffscreenCanvas(10, 10).getContext('2d');
context.font = '10px Times, testFace';
// The pre-warm helper runs unconditionally at the top of
// FontCascadeFonts::glyphDataForVariant, so any character reaches it.
context.measureText(String.fromCharCode(0x13A0));
postMessage('done');
