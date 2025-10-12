/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDRM.h"

#include "DRMUniquePtr.h"
#include <drm_fourcc.h>
#include <glib.h>
#include <string.h>
#include <wtf/TZoneMallocInlines.h>

namespace WPE {

namespace DRM {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Crtc);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Connector);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Plane);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Buffer);

static Property drmPropertyForName(int fd, drmModeObjectProperties* properties, const char* name)
{
    Property property = { 0, 0 };
    for (uint32_t i = 0; i < properties->count_props && !property.first; ++i) {
        WPE::DRM::UniquePtr<drmModePropertyRes> info(drmModeGetProperty(fd, properties->props[i]));
        if (!g_strcmp0(info->name, name)) {
            property.first = info->prop_id;
            property.second = properties->prop_values[i];
        }
    }
    return property;
}

std::unique_ptr<Crtc> Crtc::create(int fd, drmModeCrtc* crtc, unsigned index)
{
    WPE::DRM::UniquePtr<drmModeObjectProperties> properties(drmModeObjectGetProperties(fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC));
    if (!properties)
        return nullptr;

    Properties props = { drmPropertyForName(fd, properties.get(), "ACTIVE"), drmPropertyForName(fd, properties.get(), "MODE_ID") };
    return makeUnique<Crtc>(crtc, index, WTFMove(props));
}

Crtc::Crtc(drmModeCrtc* crtc, unsigned index, Properties&& properties)
    : m_id(crtc->crtc_id)
    , m_index(index)
    , m_x(crtc->x)
    , m_y(crtc->y)
    , m_width(crtc->width)
    , m_height(crtc->height)
    , m_bufferID(crtc->buffer_id)
    , m_properties(WTFMove(properties))
{
    if (crtc->mode_valid)
        m_currentMode = crtc->mode;
}

bool Crtc::modeIsCurrent(drmModeModeInfo* mode) const
{
    if (!m_currentMode)
        return false;

    return !memcmp(&m_currentMode.value(), mode, sizeof(drmModeModeInfo));
}

void Crtc::setCurrentMode(drmModeModeInfo* mode)
{
    m_currentMode = *mode;
}

std::unique_ptr<Connector> Connector::create(int fd, drmModeConnector* connector)
{
    WPE::DRM::UniquePtr<drmModeObjectProperties> properties(drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR));
    if (!properties)
        return nullptr;

    Properties props = { drmPropertyForName(fd, properties.get(), "CRTC_ID"), drmPropertyForName(fd, properties.get(), "link-status") };
    return makeUnique<Connector>(connector, WTFMove(props));
}

Connector::Connector(drmModeConnector* connector, Properties&& properties)
    : m_id(connector->connector_id)
    , m_encoderID(connector->encoder_id)
    , m_widthMM(connector->mmWidth)
    , m_heightMM(connector->mmHeight)
    , m_properties(WTFMove(properties))
{
    m_modes.reserveInitialCapacity(connector->count_modes);
    for (int i = 0; i < connector->count_modes; ++i) {
        if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED)
            m_preferredModeIndex = i;
        m_modes.append(connector->modes[i]);
    }
}

std::unique_ptr<Plane> Plane::create(int fd, Type type, drmModePlane* plane, bool modifiersSupported)
{
    WPE::DRM::UniquePtr<drmModeObjectProperties> properties(drmModeObjectGetProperties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE));
    if (!properties)
        return nullptr;

    auto typeProperty = drmPropertyForName(fd, properties.get(), "type");
    if (!typeProperty.first || typeProperty.second != static_cast<uint64_t>(type))
        return nullptr;

    Vector<Format> formats;
    formats.reserveInitialCapacity(plane->count_formats);
    bool useFallback = true;
    if (modifiersSupported) {
        auto blobID = drmPropertyForName(fd, properties.get(), "IN_FORMATS");
        if (blobID.first && blobID.second) {
            WPE::DRM::UniquePtr<drmModePropertyBlobRes> blob(drmModeGetPropertyBlob(fd, blobID.second));
            if (blob) {
                useFallback = false;

                auto* formatModifierBlob = static_cast<struct drm_format_modifier_blob*>(blob->data);
                auto* blobFormats = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(formatModifierBlob) + formatModifierBlob->formats_offset);
                auto* blobModifiers = reinterpret_cast<struct drm_format_modifier*>(reinterpret_cast<char*>(formatModifierBlob) + formatModifierBlob->modifiers_offset);
                RELEASE_ASSERT(plane->count_formats == formatModifierBlob->count_formats);

                for (uint32_t i = 0; i < formatModifierBlob->count_formats; ++i) {
                    Vector<uint64_t> modifiers;
                    modifiers.reserveInitialCapacity(formatModifierBlob->count_modifiers);

                    for (uint32_t j = 0; j < formatModifierBlob->count_modifiers; ++j) {
                        struct drm_format_modifier* modifier = &blobModifiers[j];

                        if (i < modifier->offset || i > modifier->offset + 63)
                            continue;
                        if (!(modifier->formats & (1 << (i - modifier->offset))))
                            continue;

                        modifiers.append(modifier->modifier);
                    }

                    if (modifiers.isEmpty())
                        modifiers.append(DRM_FORMAT_MOD_LINEAR);

                    formats.append({ blobFormats[i], WTFMove(modifiers) });
                }
            }
        }
    }

    if (useFallback) {
        for (uint32_t i = 0; i < plane->count_formats; ++i)
            formats.append({ plane->formats[i], { DRM_FORMAT_MOD_LINEAR } });
    }

    Properties props = {
        drmPropertyForName(fd, properties.get(), "CRTC_ID"),
        drmPropertyForName(fd, properties.get(), "CRTC_X"),
        drmPropertyForName(fd, properties.get(), "CRTC_Y"),
        drmPropertyForName(fd, properties.get(), "CRTC_W"),
        drmPropertyForName(fd, properties.get(), "CRTC_H"),
        drmPropertyForName(fd, properties.get(), "FB_ID"),
        drmPropertyForName(fd, properties.get(), "SRC_X"),
        drmPropertyForName(fd, properties.get(), "SRC_Y"),
        drmPropertyForName(fd, properties.get(), "SRC_W"),
        drmPropertyForName(fd, properties.get(), "SRC_H"),
        drmPropertyForName(fd, properties.get(), "FB_DAMAGE_CLIPS"),
        drmPropertyForName(fd, properties.get(), "IN_FENCE_FD")
    };
    return makeUnique<Plane>(plane, WTFMove(formats), WTFMove(props));
}

Plane::Plane(drmModePlane* plane, Vector<Format>&& formats, Properties&& properties)
    : m_id(plane->plane_id)
    , m_possibleCrtcs(plane->possible_crtcs)
    , m_formats(WTFMove(formats))
    , m_properties(WTFMove(properties))
{
}

bool Plane::supportsFormat(uint32_t format, uint64_t modifier) const
{
    for (const auto& planeFormat :  m_formats) {
        if (planeFormat.format != format)
            continue;

        if (modifier == DRM_FORMAT_MOD_INVALID)
            return true;

        for (auto planeModifier : planeFormat.modifiers) {
            if (planeModifier == modifier)
                return true;
        }
    }

    return false;
}

static std::optional<uint32_t> drmAddFrameBuffer(struct gbm_bo* bo)
{
    uint32_t handles[GBM_MAX_PLANES] = { 0, };
    uint32_t strides[GBM_MAX_PLANES] = { 0, };
    uint32_t offsets[GBM_MAX_PLANES] = { 0, };
    uint64_t modifiers[GBM_MAX_PLANES] = { 0, };
    if (gbm_bo_get_handle_for_plane(bo, 0).s32 == -1) {
        handles[0] = gbm_bo_get_handle(bo).u32;
        strides[0] = gbm_bo_get_stride(bo);
        offsets[0] = 0;
        modifiers[0] = DRM_FORMAT_MOD_INVALID;
    } else {
        int planeCount = gbm_bo_get_plane_count(bo);
        for (int i = 0; i < planeCount; ++i) {
            handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
            strides[i] = gbm_bo_get_stride_for_plane(bo, i);
            offsets[i] = gbm_bo_get_offset(bo, i);
            modifiers[i] = gbm_bo_get_modifier(bo);
        }
    }
    auto* device = gbm_bo_get_device(bo);
    uint32_t frameBufferID;
    if (modifiers[0] && modifiers[0] != DRM_FORMAT_MOD_INVALID) {
        if (drmModeAddFB2WithModifiers(gbm_device_get_fd(device), gbm_bo_get_width(bo), gbm_bo_get_height(bo), gbm_bo_get_format(bo), handles, strides, offsets, modifiers, &frameBufferID, DRM_MODE_FB_MODIFIERS))
            return std::nullopt;
    } else if (drmModeAddFB2(gbm_device_get_fd(device), gbm_bo_get_width(bo), gbm_bo_get_height(bo), gbm_bo_get_format(bo), handles, strides, offsets, &frameBufferID, 0))
        return std::nullopt;

    return frameBufferID;
}

std::unique_ptr<Buffer> Buffer::create(struct gbm_bo* bufferObject)
{
    auto frameBufferID = drmAddFrameBuffer(bufferObject);
    if (!frameBufferID)
        return nullptr;

    return makeUnique<Buffer>(bufferObject, *frameBufferID);
}

Buffer::Buffer(struct gbm_bo* bufferObject, uint32_t frameBufferID)
    : m_bufferObject(bufferObject)
    , m_frameBufferID(frameBufferID)
{
}

Buffer::~Buffer()
{
    drmModeRmFB(gbm_device_get_fd(gbm_bo_get_device(m_bufferObject)), m_frameBufferID);
    gbm_bo_destroy(m_bufferObject);
}

} // namespace DRM

} // namespace WPE
