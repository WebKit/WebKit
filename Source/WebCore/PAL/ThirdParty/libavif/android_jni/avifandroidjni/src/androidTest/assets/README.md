# Test images

## Sub-Directory: avif

### fox*.avif

Test files for still AVIF decoding.
[Source](https://github.com/AOMediaCodec/av1-avif/tree/77bd20d59d7cade4ab98f276ff4828433ebd089b/testFiles/Link-U).

### blue-and-magenta-crop.avif

Test file with cropping. The image has an encoded size of 320x280. If the Clean
Aperture box is honored, the size of the displayed image will be 180x100.
[Source](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/images/resources/avif/blue-and-magenta-crop.avif;l=1;drc=3a13337543c1e7de6914f87cd6f02ab06751c572).

## Sub-Directory: animated_avif

### alpha_video.avif and Chimera-AV1-10bit-480x270.avif

Test files for animated AVIF decoding.
[Source](https://github.com/AOMediaCodec/av1-avif/tree/77bd20d59d7cade4ab98f276ff4828433ebd089b/testFiles/Netflix/avis).

alpha_video.avif is patched at 0-indexed byte #259 replaced with 0x04 instead of
0x00 as [0x00 is an invalid item_id for the iref box](https://github.com/AOMediaCodec/av1-avif/issues/217).
