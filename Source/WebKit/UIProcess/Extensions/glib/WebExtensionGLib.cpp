/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"
#include "WebExtensionLocalization.h"
#include "WebExtensionUtilities.h"
#include <WebCore/DataURLDecoder.h>
#include <WebCore/LocalizedStrings.h>
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static constexpr auto generatedBackgroundPageFilename = "_generated_background_page.html"_s;
static constexpr auto generatedBackgroundServiceWorkerFilename = "_generated_service_worker.js"_s;

WebExtension::WebExtension(GFile* resourcesFile, RefPtr<API::Error>& outError)
    : m_manifestJSON(JSON::Value::null())
{
    RELEASE_ASSERT(resourcesFile);

    outError = nullptr;

    GUniquePtr<char> baseURL(g_file_get_uri(resourcesFile));
    m_resourceBaseURL = URL { makeString(String::fromUTF8(baseURL.get()), "/"_s) };

    if (m_resourceBaseURL.isValid()) {
        auto isDirectory = g_file_query_file_type(resourcesFile, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY;

        if (!isDirectory) {
            outError = createError(Error::Unknown);
            return;
        }
    }

    if (!manifestParsedSuccessfully()) {
        ASSERT(!m_errors.isEmpty());
        outError = m_errors.last().ptr();
    }
}

WebExtension::WebExtension(const JSON::Value& manifest, Resources&& resources)
    : m_manifestJSON(manifest)
    , m_resources(WTF::move(resources))
{
    auto manifestString = manifest.toJSONString();
    RELEASE_ASSERT(manifestString);

    m_resources.set("manifest.json"_s, manifestString);
}

Expected<Ref<API::Data>, RefPtr<API::Error>> WebExtension::resourceDataForPath(const String& originalPath, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    String path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);

    if (path.startsWith("data:"_s)) {
        if (auto decodedURL = WebCore::DataURLDecoder::decode(URL { path }))
            return API::Data::create(decodedURL.value().data);
        ASSERT(path == "data:"_s);
        return API::Data::create(std::span<const uint8_t> { });
    }

    if (path == generatedBackgroundPageFilename || path  == generatedBackgroundServiceWorkerFilename)
        return API::Data::create(generatedBackgroundContent().utf8().span());

    if (auto entry = m_resources.find(path); entry != m_resources.end()) {
        return WTF::switchOn(entry->value,
            [](const Ref<API::Data>& data) {
                return data;
            },
            [](const String& string) {
                return API::Data::create(string.utf8().span());
            });
    }

    auto resourceURL = resourceFileURLForPath(path);
    if (resourceURL.isEmpty()) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            return makeUnexpected(createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", path.utf8().data())));
        return makeUnexpected(nullptr);
    }

    auto rawData = FileSystem::readEntireFile(resourceURL.fileSystemPath());
    if (!rawData.has_value()) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            return makeUnexpected(createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", path.utf8().data())));
        return makeUnexpected(nullptr);
    }

    Ref data = API::Data::create(*rawData);
    if (cacheResult == CacheResult::Yes)
        m_resources.set(path, data);

    return data;
}

void WebExtension::recordError(Ref<API::Error> error)
{
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error->localizedDescription().utf8().data());

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.contains(error))
        return;

    m_errors.append(error);
}

RefPtr<WebCore::Icon> WebExtension::bestIcon(RefPtr<JSON::Object> icons, WebCore::FloatSize idealSize, NOESCAPE const Function<void(Ref<API::Error>)>& reportError)
{
    if (!icons)
        return nullptr;

    auto idealPointSize = idealSize.width() > idealSize.height() ? idealSize.width() : idealSize.height();
    auto bestScale = largestDisplayScale();

    auto pixelSize = idealPointSize * bestScale;
    auto iconPath = pathForBestImage(*icons, pixelSize);
    if (iconPath.isEmpty())
        return nullptr;

    auto imageValue = iconForPath(iconPath, idealSize);
    if (imageValue)
        return imageValue.value().get();

    if (reportError && !imageValue && imageValue.error())
        reportError(imageValue.error().releaseNonNull());

    return nullptr;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
