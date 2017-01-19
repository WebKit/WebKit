# Introduction

Rotation by multiplies of 90 degrees allows mobile devices to rotate webcams from landscape to portrait.  The higher level functions ConvertToI420 and ConvertToARGB allow rotation of any format.  Optimized functionality is supported for I420, ARGB, NV12 and NV21.

# ConvertToI420

    int ConvertToI420(const uint8* src_frame, size_t src_size,
                      uint8* dst_y, int dst_stride_y,
                      uint8* dst_u, int dst_stride_u,
                      uint8* dst_v, int dst_stride_v,
                      int crop_x, int crop_y,
                      int src_width, int src_height,
                      int crop_width, int crop_height,
                      enum RotationMode rotation,
                      uint32 format);

This function crops, converts, and rotates.  You should think of it in that order.
  * Crops the original image, which is src_width x src_height, to crop_width x crop_height.  At this point the image is still not rotated.
  * Converts the cropped region to I420.  Supports inverted source for src_height negative.
  * Rotates by 90, 180 or 270 degrees.
The buffer the caller provides should account for rotation.  Be especially important to get stride of the destination correct.

e.g.
640 x 480 NV12 captured<br>
Crop to 640 x 360<br>
Rotate by 90 degrees to 360 x 640.<br>
Caller passes stride of 360 for Y and 360 / 2 for U and V.<br>
Caller passes crop_width of 640, crop_height of 360.<br>

# ConvertToARGB

    int ConvertToARGB(const uint8* src_frame, size_t src_size,
                      uint8* dst_argb, int dst_stride_argb,
                      int crop_x, int crop_y,
                      int src_width, int src_height,
                      int crop_width, int crop_height,
                      enum RotationMode rotation,
                      uint32 format);

Same as I420, but implementation is less optimized - reads columns and writes rows, 16 bytes at a time.

# I420Rotate

    int I420Rotate(const uint8* src_y, int src_stride_y,
                   const uint8* src_u, int src_stride_u,
                   const uint8* src_v, int src_stride_v,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int src_width, int src_height, enum RotationMode mode);

Destination is rotated, so pass dst_stride_y etc that consider rotation.<br>
Rotate by 180 can be done in place, but 90 and 270 can not.

Implementation (Neon/SSE2) uses 8 x 8 block transpose, so best efficiency is with sizes and pointers that are aligned to 8.

Cropping can be achieved by adjusting the src_y/u/v pointers and src_width, src_height.

Lower level plane functions are provided, allowing other planar formats to be rotated.  (e.g. I444)

For other planar YUV formats (I444, I422, I411, I400, NV16, NV24), the planar functions are exposed and can be called directly


    // Rotate a plane by 0, 90, 180, or 270.
    int RotatePlane(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int src_width, int src_height, enum RotationMode mode);

# ARGBRotate

    LIBYUV_API
    int ARGBRotate(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb, int dst_stride_argb,
                   int src_width, int src_height, enum RotationMode mode);

Same as I420, but implementation is less optimized - reads columns and writes rows.

Rotate by 90, or any angle, can be achieved using ARGBAffine.

# Mirror - Horizontal Flip

Mirror functions for horizontally flipping an image, which can be useful for 'self view' of a webcam.

    int I420Mirror(const uint8* src_y, int src_stride_y,
                   const uint8* src_u, int src_stride_u,
                   const uint8* src_v, int src_stride_v,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int width, int height);
    int ARGBMirror(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb, int dst_stride_argb,
                   int width, int height);

Mirror functionality can also be achieved with the I420Scale and ARGBScale functions by passing negative width and/or height.

# Invert - Vertical Flip

Inverting can be achieved with almost any libyuv function by passing a negative source height.

I420Mirror and ARGBMirror can also be used to rotate by 180 degrees by passing a negative height.


