/* Copyright 2020 Emmanuel Gil Peyrot. All rights reserved.
   SPDX-License-Identifier: BSD-2-Clause
*/

#include <avif/avif.h>
#include <stdlib.h>

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_MODULE_EXPORT void fill_vtable (GdkPixbufModule * module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat * info);

struct avif_context {
    GdkPixbuf * pixbuf;

    GdkPixbufModuleSizeFunc size_func;
    GdkPixbufModuleUpdatedFunc updated_func;
    GdkPixbufModulePreparedFunc prepared_func;
    gpointer user_data;

    avifDecoder * decoder;
    GByteArray * data;
    GBytes * bytes;
};

static void avif_context_free(struct avif_context * context)
{
    if (!context)
        return;

    if (context->decoder) {
        avifDecoderDestroy(context->decoder);
        context->decoder = NULL;
    }

    if (context->data) {
        g_byte_array_unref(context->data);
        context->bytes = NULL;
    }

    if (context->bytes) {
        g_bytes_unref(context->bytes);
        context->bytes = NULL;
    }

    if (context->pixbuf) {
        g_object_unref(context->pixbuf);
        context->pixbuf = NULL;
    }

    g_free(context);
}

static gboolean avif_context_try_load(struct avif_context * context, GError ** error)
{
    avifResult ret;
    avifDecoder * decoder = context->decoder;
    avifImage * image;
    avifRGBImage rgb;
    const uint8_t * data;
    size_t size;
    int width, height;
    GdkPixbuf *output;

    data = g_bytes_get_data(context->bytes, &size);

    ret = avifDecoderSetIOMemory(decoder, data, size);
    if (ret != AVIF_RESULT_OK) {
        g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                    "Couldn't decode image: %s", avifResultToString(ret));
        return FALSE;
    }

    ret = avifDecoderParse(decoder);
    if (ret != AVIF_RESULT_OK) {
        g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                    "Couldn't decode image: %s", avifResultToString(ret));
        return FALSE;
    }

    if (decoder->imageCount > 1) {
        g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                            "Image sequences not yet implemented");
        return FALSE;
    }

    ret = avifDecoderNextImage(decoder);
    if (ret == AVIF_RESULT_NO_IMAGES_REMAINING) {
        /* No more images, bail out. Verify that you got the expected amount of images decoded. */
        return TRUE;
    } else if (ret != AVIF_RESULT_OK) {
        g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                    "Failed to decode all frames: %s", avifResultToString(ret));
        return FALSE;
    }

    image = decoder->image;
    width = image->width;
    height = image->height;

    avifRGBImageSetDefaults(&rgb, image);
    rgb.depth = 8;

    if (image->alphaPlane) {
        rgb.format = AVIF_RGB_FORMAT_RGBA;
        output = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                TRUE, 8, width, height);
    } else {
        rgb.format = AVIF_RGB_FORMAT_RGB;
        output = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                FALSE, 8, width, height);
    }

    if (output == NULL) {
        g_set_error_literal(error,
                            GDK_PIXBUF_ERROR,
                            GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                            "Insufficient memory to open AVIF file");
        return FALSE;
    }

    rgb.pixels = gdk_pixbuf_get_pixels(output);
    rgb.rowBytes = gdk_pixbuf_get_rowstride(output);

    ret = avifImageYUVToRGB(image, &rgb);
    if (ret != AVIF_RESULT_OK) {
        g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                    "Failed to convert YUV to RGB: %s", avifResultToString(ret));
        return FALSE;
    }

    /* transformations */
    if (image->transformFlags & AVIF_TRANSFORM_CLAP) {
        if ((image->clap.widthD > 0) && (image->clap.heightD > 0) &&
            (image->clap.horizOffD > 0) && (image->clap.vertOffD > 0)) {

            int new_width, new_height;

            new_width = (int)((double)(image->clap.widthN)  / (image->clap.widthD) + 0.5);
            if (new_width > width) {
                new_width = width;
            }

            new_height = (int)((double)(image->clap.heightN) / (image->clap.heightD) + 0.5);
            if (new_height > height) {
                new_height = height;
            }

            if (new_width > 0 && new_height > 0) {
                int offx, offy;
                GdkPixbuf *output_cropped;
                GdkPixbuf *cropped_copy;

                offx = ((double)((int32_t) image->clap.horizOffN)) / (image->clap.horizOffD) +
                       (width - new_width) / 2.0 + 0.5;
                if (offx < 0) {
                    offx = 0;
                } else if (offx > (width - new_width)) {
                    offx = width - new_width;
                }

                offy = ((double)((int32_t) image->clap.vertOffN)) / (image->clap.vertOffD) +
                       (height - new_height) / 2.0 + 0.5;
                if (offy < 0) {
                    offy = 0;
                } else if (offy > (height - new_height)) {
                    offy = height - new_height;
                }

                output_cropped = gdk_pixbuf_new_subpixbuf(output, offx, offy, new_width, new_height);
                cropped_copy = gdk_pixbuf_copy(output_cropped);
                g_clear_object(&output_cropped);

                if (cropped_copy) {
                    g_object_unref(output);
                    output = cropped_copy;
                }
            }
        } else {
            /* Zero values, we need to avoid 0 divide. */
            g_warning("Wrong values in avifCleanApertureBox\n");
        }
    }

    if (image->transformFlags & AVIF_TRANSFORM_IROT) {
        GdkPixbuf *output_rotated = NULL;

        switch (image->irot.angle) {
        case 1:
            output_rotated = gdk_pixbuf_rotate_simple(output, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        case 2:
            output_rotated = gdk_pixbuf_rotate_simple(output, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case 3:
            output_rotated = gdk_pixbuf_rotate_simple(output, GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        }

        if (output_rotated) {
          g_object_unref(output);
          output = output_rotated;
        }
    }

    if (image->transformFlags & AVIF_TRANSFORM_IMIR) {
        GdkPixbuf *output_mirrored = NULL;

        switch (image->imir.mode) {
        case 0:
            output_mirrored = gdk_pixbuf_flip(output, FALSE);
            break;
        case 1:
            output_mirrored = gdk_pixbuf_flip(output, TRUE);
            break;
        }

        if (output_mirrored) {
            g_object_unref(output);
            output = output_mirrored;
        }
    }

    /* width, height could be different after applied transformations */
    width = gdk_pixbuf_get_width(output);
    height = gdk_pixbuf_get_height(output);

    if (context->size_func) {
        (*context->size_func)(&width, &height, context->user_data);
    }

    if (width == 0 || height == 0) {
        g_set_error_literal(error,
                            GDK_PIXBUF_ERROR,
                            GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                            "Transformed AVIF has zero width or height");
        return FALSE;
    }

    if ( width < gdk_pixbuf_get_width(output) ||
         height < gdk_pixbuf_get_height(output)) {
        GdkPixbuf *output_scaled = NULL;

        output_scaled = gdk_pixbuf_scale_simple(output, width, height, GDK_INTERP_HYPER);
        if (output_scaled) {
            g_object_unref(output);
            output = output_scaled;
        }
    }

    if (context->pixbuf) {
        g_object_unref(context->pixbuf);
        context->pixbuf = NULL;
    }

    context->pixbuf = output;
    context->prepared_func(context->pixbuf, NULL, context->user_data);

    return TRUE;
}

static gpointer begin_load(GdkPixbufModuleSizeFunc size_func,
                           GdkPixbufModulePreparedFunc prepared_func,
                           GdkPixbufModuleUpdatedFunc updated_func,
                           gpointer user_data, GError ** error)
{
    struct avif_context * context;
    avifDecoder * decoder;

    g_assert(prepared_func != NULL);

    decoder = avifDecoderCreate();
    if (!decoder) {
        g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                            "Couldn't allocate memory for decoder");
        return NULL;
    }

    context = g_new0(struct avif_context, 1);
    if (!context)
        return NULL;

    context->size_func = size_func;
    context->updated_func = updated_func;
    context->prepared_func = prepared_func;
    context->user_data = user_data;

    context->decoder = decoder;
    context->data = g_byte_array_sized_new(40000);

    return context;
}

static gboolean stop_load(gpointer data, GError ** error)
{
    struct avif_context * context = (struct avif_context *) data;
    gboolean ret;

    context->bytes = g_byte_array_free_to_bytes(context->data);
    context->data = NULL;
    ret = avif_context_try_load(context, error);

    avif_context_free(context);

    return ret;
}

static gboolean load_increment(gpointer data, const guchar * buf, guint size, GError ** error)
{
    struct avif_context * context = (struct avif_context *) data;
    g_byte_array_append(context->data, buf, size);
    if (error)
        *error = NULL;
    return TRUE;
}

static gboolean avif_is_save_option_supported (const gchar *option_key)
{
    if (g_strcmp0(option_key, "quality") == 0) {
        return TRUE;
    }

    return FALSE;
}

static gboolean avif_image_saver(FILE          *f,
                                GdkPixbuf     *pixbuf,
                                gchar        **keys,
                                gchar        **values,
                                GError       **error)
{
    int width, height, min_quantizer, max_quantizer, alpha_quantizer;
    long quality = 52; /* default; must be between 0 and 100 */
    gboolean save_alpha;
    avifImage *avif;
    avifRGBImage rgb;
    avifResult res;
    avifRWData raw = AVIF_DATA_EMPTY;
    avifEncoder *encoder;
    guint maxThreads;

    if (f == NULL || pixbuf == NULL) {
        return FALSE;
    }

    if (keys && *keys) {
        gchar **kiter = keys;
        gchar **viter = values;

        while (*kiter) {
            if (strcmp(*kiter, "quality") == 0) {
                char *endptr = NULL;
                quality = strtol(*viter, &endptr, 10);

                if (endptr == *viter) {
                    g_set_error(error,
                                GDK_PIXBUF_ERROR,
                                GDK_PIXBUF_ERROR_BAD_OPTION,
                                "AVIF quality must be a value between 0 and 100; value \"%s\" could not be parsed.",
                                *viter);

                    return FALSE;
                }

                if (quality < 0 || quality > 100) {
                    g_set_error(error,
                                GDK_PIXBUF_ERROR,
                                GDK_PIXBUF_ERROR_BAD_OPTION,
                                "AVIF quality must be a value between 0 and 100; value \"%ld\" is not allowed.",
                                quality);

                    return FALSE;
                }
            } else {
                g_warning("Unrecognized parameter (%s) passed to AVIF saver.", *kiter);
            }

            ++kiter;
            ++viter;
        }
    }

    if (gdk_pixbuf_get_bits_per_sample(pixbuf) != 8) {
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                    "Sorry, only 8bit images are supported by this AVIF saver");
        return FALSE;
    }

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    if ( width == 0 || height == 0) {
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                    "Empty image, nothing to save");
        return FALSE;
    }

    save_alpha = gdk_pixbuf_get_has_alpha(pixbuf);

    if (save_alpha) {
        if ( gdk_pixbuf_get_n_channels(pixbuf) != 4) {
            g_set_error(error,
                        GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                        "Unsupported number of channels");
            return FALSE;
        }
    }
    else {
        if ( gdk_pixbuf_get_n_channels(pixbuf) != 3) {
            g_set_error(error,
                        GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                        "Unsupported number of channels");
            return FALSE;
        }
    }

    max_quantizer = AVIF_QUANTIZER_WORST_QUALITY * (100 - (int)quality) / 100;
    min_quantizer = 0;
    alpha_quantizer = 0;

    if ( max_quantizer > 20 ) {
        min_quantizer = max_quantizer - 20;

        if (max_quantizer > 40) {
            alpha_quantizer = max_quantizer - 40;
        }
    }

    avif = avifImageCreate(width, height, 8, AVIF_PIXEL_FORMAT_YUV420);
    avif->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    avifRGBImageSetDefaults( &rgb, avif);

    rgb.depth = 8;
    rgb.pixels = (uint8_t*) gdk_pixbuf_read_pixels(pixbuf);
    rgb.rowBytes = gdk_pixbuf_get_rowstride(pixbuf);

    if (save_alpha) {
        rgb.format = AVIF_RGB_FORMAT_RGBA;
    } else {
        rgb.format = AVIF_RGB_FORMAT_RGB;
    }

    res = avifImageRGBToYUV(avif, &rgb);
    if ( res != AVIF_RESULT_OK ) {
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_FAILED,
                    "Problem in RGB->YUV conversion: %s", avifResultToString(res));
        avifImageDestroy(avif);
        return FALSE;
    }

    maxThreads = g_get_num_processors();
    encoder = avifEncoderCreate();

    encoder->maxThreads = CLAMP(maxThreads, 1, 64);
    encoder->minQuantizer = min_quantizer;
    encoder->maxQuantizer = max_quantizer;
    encoder->minQuantizerAlpha = 0;
    encoder->maxQuantizerAlpha = alpha_quantizer;
    encoder->speed = 7;

    res = avifEncoderWrite(encoder, avif, &raw);
    avifEncoderDestroy(encoder);
    avifImageDestroy(avif);

    if ( res == AVIF_RESULT_OK ) {
        fwrite(raw.data, 1, raw.size, f);
        avifRWDataFree(&raw);
        return TRUE;
    }

    g_set_error(error,
                GDK_PIXBUF_ERROR,
                GDK_PIXBUF_ERROR_FAILED,
                "AVIF encoder problem: %s", avifResultToString(res));
    return FALSE;
}


G_MODULE_EXPORT void fill_vtable(GdkPixbufModule * module)
{
    module->begin_load = begin_load;
    module->stop_load = stop_load;
    module->load_increment = load_increment;
    module->is_save_option_supported = avif_is_save_option_supported;
    module->save = avif_image_saver;
}

G_MODULE_EXPORT void fill_info(GdkPixbufFormat * info)
{
    static GdkPixbufModulePattern signature[] = {
        { "    ftypavif", "zzz         ", 100 },    /* file begins with 'ftypavif' at offset 4 */
        { NULL, NULL, 0 }
    };
    static gchar * mime_types[] = {
        "image/avif",
        NULL
    };
    static gchar * extensions[] = {
        "avif",
        NULL
    };

    info->name = "avif";
    info->signature = (GdkPixbufModulePattern *)signature;
    info->description = "AV1 Image File Format";
    info->mime_types = (gchar **)mime_types;
    info->extensions = (gchar **)extensions;
    info->flags = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license = "BSD";
    info->disabled = FALSE;
}

