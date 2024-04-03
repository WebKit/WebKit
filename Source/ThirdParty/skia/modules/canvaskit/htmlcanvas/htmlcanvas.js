CanvasKit.MakeCanvas = function(width, height) {
  var surf = CanvasKit.MakeSurface(width, height);
  if (surf) {
    return new HTMLCanvas(surf);
  }
  return null;
};

function HTMLCanvas(skSurface) {
  this._surface = skSurface;
  this._context = new CanvasRenderingContext2D(skSurface.getCanvas());
  this._toCleanup = [];

  // Data is either an ArrayBuffer, a TypedArray, or a Node Buffer
  this.decodeImage = function(data) {
    var img = CanvasKit.MakeImageFromEncoded(data);
    if (!img) {
      throw 'Invalid input';
    }
    this._toCleanup.push(img);
    return new HTMLImage(img);
  };

  this.loadFont = function(buffer, descriptors) {
    var newFont = CanvasKit.Typeface.MakeTypefaceFromData(buffer);
    if (!newFont) {
      Debug('font could not be processed', descriptors);
      return null;
    }
    this._toCleanup.push(newFont);
    addToFontCache(newFont, descriptors);
  };

  this.makePath2D = function(path) {
    var p2d = new Path2D(path);
    this._toCleanup.push(p2d._getPath());
    return p2d;
  };

  // A normal <canvas> requires that clients call getContext
  this.getContext = function(type) {
    if (type === '2d') {
      return this._context;
    }
    return null;
  };

  this.toDataURL = function(codec, quality) {
    // TODO(kjlubick): maybe support other codecs (webp?)
    // For now, just to png and jpeg
    this._surface.flush();

    var img = this._surface.makeImageSnapshot();
    if (!img) {
      Debug('no snapshot');
      return;
    }
    codec = codec || 'image/png';
    var format = CanvasKit.ImageFormat.PNG;
    if (codec === 'image/jpeg') {
      format = CanvasKit.ImageFormat.JPEG;
    }
    quality = quality || 0.92;
    var imgBytes = img.encodeToBytes(format, quality);
    if (!imgBytes) {
      Debug('encoding failure');
      return
    }
    img.delete();
    return 'data:' + codec + ';base64,' + toBase64String(imgBytes);
  };

  this.dispose = function() {
    this._context._dispose();
    this._toCleanup.forEach(function(i) {
      i.delete();
    });
    this._surface.dispose();
  }
}
