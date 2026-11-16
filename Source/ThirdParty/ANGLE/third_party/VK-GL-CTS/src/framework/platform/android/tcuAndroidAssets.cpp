/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Resource wrapper for AAssetManager.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidAssets.hpp"

namespace tcu
{
namespace Android
{

AssetArchive::AssetArchive(AAssetManager *assetMgr) : m_assetMgr(assetMgr)
{
}

AssetArchive::~AssetArchive(void)
{
}

Resource *AssetArchive::getResource(const char *name) const
{
    return new AssetResource(m_assetMgr, name);
}

AssetResource::AssetResource(AAssetManager *assetMgr, const char *name) : Resource(name), m_asset(nullptr)
{
    m_asset = AAssetManager_open(assetMgr, name, AASSET_MODE_RANDOM);

    if (!m_asset)
        throw ResourceError("Failed to open asset resource", name, __FILE__, __LINE__);
}

AssetResource::~AssetResource(void)
{
    AAsset_close(m_asset);
}

void AssetResource::read(uint8_t *dst, int numBytes)
{
    TCU_CHECK(AAsset_read(m_asset, dst, numBytes) == numBytes);
}

uint32_t AssetResource::getPosition(void) const
{
    return (int)AAsset_getLength(m_asset) - (int)AAsset_getRemainingLength(m_asset);
}

void AssetResource::setPosition(uint32_t position)
{
    TCU_CHECK(AAsset_seek(m_asset, position, SEEK_SET) == (off_t)position);
}

bool AssetResource::isFinished(void) const
{
    return AAsset_getRemainingLength(m_asset) <= 0;
}

uint32_t AssetResource::getSize(void) const
{
    return (int)AAsset_getLength(m_asset);
}

} // namespace Android
} // namespace tcu
