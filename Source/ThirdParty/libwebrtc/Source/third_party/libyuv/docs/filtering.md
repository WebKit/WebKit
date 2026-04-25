# Introduction

This document discusses the current state of filtering in libyuv. An emphasis on maximum performance while avoiding memory exceptions, and minimal amount of code/complexity.  See future work at end.

# LibYuv Filter Subsampling

There are 2 challenges with subsampling

1. centering of samples, which involves clamping on edges
2. clipping a source region

Centering depends on scale factor and filter mode.

# Down Sampling

If scaling down, the stepping rate is always src_width / dst_width.

    dx = src_width / dst_width;

e.g. If scaling from 1280x720 to 640x360, the step thru the source will be 2.0, stepping over 2 pixels of source for each pixel of destination.

Centering, depends on filter mode.

*Point* downsampling takes the middle pixel.

    x = dx >> 1;

For odd scale factors (e.g. 3x down) this is exactly the middle.  For even scale factors, this rounds up and takes the pixel to the right of center.  e.g. scale of 4x down will take pixel 2.

**Bilinear** filter, uses the 2x2 pixels in the middle.

    x = dx / 2 - 0.5;

For odd scale factors (e.g. 3x down) this is exactly the middle, and point sampling is used.
For even scale factors, this evenly filters the middle 2x2 pixels.  e.g. 4x down will filter pixels 1,2 at 50% in both directions.

**Box** filter averages the entire box so sampling starts at 0.

    x = 0;

For a scale factor of 2x down, this is equivalent to bilinear.

# Up Sampling

**Point** upsampling use stepping rate of src_width / dst_width and a starting coordinate of 0.

    x = 0;
    dx = src_width / dst_width;

e.g. If scaling from 640x360 to 1280x720 the step thru the source will be 0.0, stepping half a pixel of source for each pixel of destination. Each pixel is replicated by the scale factor.

**Bilinear** filter stretches such that the first pixel of source maps to the first pixel of destination, and the last pixel of source maps to the last pixel of destination.

    x = 0;
    dx = (src_width - 1) / (dst_width - 1);

This method is not technically correct, and will likely change in the future.

* It is inconsistent with the bilinear down sampler.  The same method could be used for down sampling, and then it would be more reversible, but that would prevent specialized 2x down sampling.
* Although centered, the image is slightly magnified.
* The filtering was changed in early 2013 - previously it used:

        x = 0;
        dx = (src_width - 1) / (dst_width - 1);

Which is the correct scale factor, but shifted the image left, and extruded the last pixel.  The reason for the change was to remove the extruding code from the low level row functions, allowing 3 functions to sshare the same row functions - ARGBScale, I420Scale, and ARGBInterpolate.  Then the one function was ported to many cpu variations: SSE2, SSSE3, AVX2, Neon and 'Any' version for any number of pixels and alignment.  The function is also specialized for 0,25,50,75%.

The above goes still has the potential to read the last pixel 100% and last pixel + 1 0%, which may cause a memory exception.  So the left pixel goes to a fraction less than the last pixel, but filters in the minimum amount of it, and the maximum of the last pixel.

    dx = FixedDiv((src_width << 16) - 0x00010001, (dst << 16) - 0x00010000);

**Box** filter for upsampling switches over to Bilinear.

# Scale snippet:

    #define CENTERSTART(dx, s) (dx < 0) ? -((-dx >> 1) + s) : ((dx >> 1) + s)
    #define FIXEDDIV1(src, dst) FixedDiv((src << 16) - 0x00010001, \
                                         (dst << 16) - 0x00010000);

    // Compute slope values for stepping.
    void ScaleSlope(int src_width, int src_height,
                    int dst_width, int dst_height,
                    FilterMode filtering,
                    int* x, int* y, int* dx, int* dy) {
      assert(x != NULL);
      assert(y != NULL);
      assert(dx != NULL);
      assert(dy != NULL);
      assert(src_width != 0);
      assert(src_height != 0);
      assert(dst_width > 0);
      assert(dst_height > 0);
      if (filtering == kFilterBox) {
        // Scale step for point sampling duplicates all pixels equally.
        *dx = FixedDiv(Abs(src_width), dst_width);
        *dy = FixedDiv(src_height, dst_height);
        *x = 0;
        *y = 0;
      } else if (filtering == kFilterBilinear) {
        // Scale step for bilinear sampling renders last pixel once for upsample.
        if (dst_width <= Abs(src_width)) {
          *dx = FixedDiv(Abs(src_width), dst_width);
          *x = CENTERSTART(*dx, -32768);
        } else if (dst_width > 1) {
          *dx = FIXEDDIV1(Abs(src_width), dst_width);
          *x = 0;
        }
        if (dst_height <= src_height) {
          *dy = FixedDiv(src_height,  dst_height);
          *y = CENTERSTART(*dy, -32768);  // 32768 = -0.5 to center bilinear.
        } else if (dst_height > 1) {
          *dy = FIXEDDIV1(src_height, dst_height);
          *y = 0;
        }
      } else if (filtering == kFilterLinear) {
        // Scale step for bilinear sampling renders last pixel once for upsample.
        if (dst_width <= Abs(src_width)) {
          *dx = FixedDiv(Abs(src_width), dst_width);
          *x = CENTERSTART(*dx, -32768);
        } else if (dst_width > 1) {
          *dx = FIXEDDIV1(Abs(src_width), dst_width);
          *x = 0;
        }
        *dy = FixedDiv(src_height, dst_height);
        *y = *dy >> 1;
      } else {
        // Scale step for point sampling duplicates all pixels equally.
        *dx = FixedDiv(Abs(src_width), dst_width);
        *dy = FixedDiv(src_height, dst_height);
        *x = CENTERSTART(*dx, 0);
        *y = CENTERSTART(*dy, 0);
      }
      // Negative src_width means horizontally mirror.
      if (src_width < 0) {
        *x += (dst_width - 1) * *dx;
        *dx = -*dx;
        src_width = -src_width;
      }
    }

# Future Work

Point sampling should ideally be the same as bilinear, but pixel by pixel, round to nearest neighbor.  But as is, it is reversible and exactly matches ffmpeg at all scale factors, both up and down.  The scale factor is

    dx = src_width / dst_width;

The step value is centered for down sample:

    x = dx / 2;

Or starts at 0 for upsample.

    x = 0;

Bilinear filtering is currently correct for down sampling, but not for upsampling.
Upsampling is stretching the first and last pixel of source, to the first and last pixel of destination.

    dx = (src_width - 1) / (dst_width - 1);<br>
    x = 0;

It should be stretching such that the first pixel is centered in the middle of the scale factor, to match the pixel that would be sampled for down sampling by the same amount.  And same on last pixel.

    dx = src_width / dst_width;<br>
    x = dx / 2 - 0.5;

This would start at -0.5 and go to last pixel + 0.5, sampling 50% from last pixel + 1.
Then clamping would be needed.  On GPUs there are numerous ways to clamp.

1. Clamp the coordinate to the edge of the texture, duplicating the first and last pixel.
2. Blend with a constant color, such as transparent black.  Typically best for fonts.
3. Mirror the UV coordinate, which is similar to clamping.  Good for continuous tone images.
4. Wrap the coordinate, for texture tiling.
5. Allow the coordinate to index beyond the image, which may be the correct data if sampling a subimage.
6. Extrapolate the edge based on the previous pixel.  pixel -0.5 is computed from slope of pixel 0 and 1.

Some of these are computational, even for a GPU, which is one reason textures are sometimes limited to power of 2 sizes.
We do care about the clipping case, where allowing coordinates to become negative and index pixels before the image is the correct data.  But normally for simple scaling, we want to clamp to the edge pixel.  For example, if bilinear scaling from 3x3 to 30x30, we’d essentially want 10 pixels of each of the original 3 pixels.  But we want the original pixels to land in the middle of each 10 pixels, at offsets 5, 15 and 25.  There would be filtering between 5 and 15 between the original pixels 0 and 1.  And filtering between 15 and 25 from original pixels 1 and 2.  The first 5 pixels are clamped to pixel 0 and the last 5 pixels are clamped to pixel 2.
The easiest way to implement this is copy the original 3 pixels to a buffer, and duplicate the first and last pixels.  0,1,2 becomes 0, 0,1,2, 2.  Then implement a filtering without clamping.  We call this source extruding.  Its only necessary on up sampling, since down sampler will always have valid surrounding pixels.
Extruding is practical when the image is already copied to a temporary buffer.   It could be done to the original image, as long as the original memory is restored, but valgrind and/or memory protection would disallow this, so it requires a memcpy to a temporary buffer, which may hurt performance.  The memcpy has a performance advantage, from a cache point of view, that can actually make this technique faster, depending on hardware characteristics.
Vertical extrusion can be done with a memcpy of the first/last row, or clamping a pointer.


The other way to implement clamping is handle the edges with a memset.  e.g. Read first source pixel and memset the first 5 pixels.  Filter pixels 0,1,2 to 5 to 25.  Read last pixel and memset the last 5 pixels.  Blur is implemented with this method like this, which has 3 loops per row - left, middle and right.

Box filter is only used for 2x down sample or more.  Its based on integer sized boxes.  Technically it should be filtered edges, but thats substantially slower (roughly 100x), and at that point you may as well do a cubic filter which is more correct.

Box filter currently sums rows into a row buffer.  It does this with

Mirroring will use the same slope as normal, but with a negative.
The starting coordinate needs to consider the scale factor and filter.  e.g. box filter of 30x30 to 3x3 with mirroring would use -10 for step, but x = 20.  width (30) - dx.

Step needs to be accurate, so it uses an integer divide.  This is as much as 5% of the profile.  An approximated divide is substantially faster, but the inaccuracy causes stepping beyond the original image boundaries.  3 general solutions:

1. copy image to buffer with padding.  allows for small errors in stepping.
2. hash the divide, so common values are quickly found.
3. change api so caller provides the slope.
