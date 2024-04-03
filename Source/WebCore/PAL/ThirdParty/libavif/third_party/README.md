# libavif third party sources

Anything in this directory is imported from third party sources with minimal changes.

See `LICENSE` file of respective subdirectories of this directory for license information.

## libyuv

This subdirectory contains source code imported from libyuv as of `def473f501acbd652cd4593fd2a90a067e8c9f1a`, with
modifications intended to keep them relatively small and simple.

### Importing from upstream

When importing source code from upstream libyuv, the following changes must be done:

1. Source hierarchy is to be kept the same as in libyuv.
2. The only APIs that are called by libavif for scaling are `ScalePlane()` and `ScalePlane_12()`. Anything else must be
   left out.
3. In function `ScalePlane()` and `ScalePlane_16()`, only the `ScalePlaneVertical()`, `CopyPlane()`, `ScalePlaneBox()`,
   `ScalePlaneUp2_Linear()`, `ScalePlaneUp2_Bilinear()`, `ScalePlaneBilinearUp()`, `ScalePlaneBilinearDown()` and
   `ScalePlaneSimple()` paths (and their `_16` equivalents) from libyuv must be kept; any other paths are to be stripped
   out (including SIMD).
4. The commit hash of libyuv from where the files got imported in this README.md file must be updated.
5. `LIBYUV_API` must be removed from any and all imported functions as these files are always built static.
6. Replace .cc file extension by .c.
