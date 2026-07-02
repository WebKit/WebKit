#ifndef _TCUANDROIDASSETS_HPP
#define _TCUANDROIDASSETS_HPP
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

#include "tcuDefs.hpp"
#include "tcuResource.hpp"

#include <android/asset_manager.h>

namespace tcu
{
namespace Android
{

class AssetArchive : public Archive
{
public:
    AssetArchive(AAssetManager *assetMgr);
    ~AssetArchive(void);

    Resource *getResource(const char *name) const;

private:
    AAssetManager *m_assetMgr;
};

class AssetResource : public Resource
{
public:
    AssetResource(AAssetManager *assetMgr, const char *name);
    ~AssetResource(void);

    void read(uint8_t *dst, int numBytes);
    uint32_t getPosition(void) const;
    void setPosition(uint32_t position);
    bool isFinished(void) const;
    uint32_t getSize(void) const;

private:
    AssetResource(const AssetResource &other);
    AssetResource operator=(const AssetResource &other);

    AAsset *m_asset;
};

} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDASSETS_HPP
