/*
 *  Copyright (C) 2012 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Logging.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#define LOG_MEDIA_MESSAGE(...) do { \
    GST_DEBUG(__VA_ARGS__); \
    LOG_VERBOSE(Media, __VA_ARGS__); } while (0)

#define ERROR_MEDIA_MESSAGE(...) do { \
    GST_ERROR(__VA_ARGS__); \
    LOG_VERBOSE(Media, __VA_ARGS__); } while (0)

#define INFO_MEDIA_MESSAGE(...) do { \
    GST_INFO(__VA_ARGS__); \
    LOG_VERBOSE(Media, __VA_ARGS__); } while (0)

#define WARN_MEDIA_MESSAGE(...) do { \
    GST_WARNING(__VA_ARGS__); \
    LOG_VERBOSE(Media, __VA_ARGS__); } while (0)

namespace WebCore {

class IntSize;

inline bool webkitGstCheckVersion(guint major, guint minor, guint micro)
{
    guint currentMajor, currentMinor, currentMicro, currentNano;
    gst_version(&currentMajor, &currentMinor, &currentMicro, &currentNano);

    if (currentMajor < major)
        return false;
    if (currentMajor > major)
        return true;

    if (currentMinor < minor)
        return false;
    if (currentMinor > minor)
        return true;

    if (currentMicro < micro)
        return false;

    return true;
}

GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate*, const gchar* name, GstPad* target);
#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(GstCaps*, WebCore::IntSize&, GstVideoFormat&, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride);
#endif
GstBuffer* createGstBuffer(GstBuffer*);
GstBuffer* createGstBufferForData(const char* data, int length);
char* getGstBufferDataPointer(GstBuffer*);
void mapGstBuffer(GstBuffer*);
void unmapGstBuffer(GstBuffer*);
bool initializeGStreamer();
unsigned getGstPlayFlag(const char* nick);
GstClockTime toGstClockTime(float time);

}
