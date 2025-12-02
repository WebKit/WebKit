/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(WK_WEB_EXTENSIONS)

#import <WebCore/Icon.h>
#import <wtf/HashSet.h>
#import <wtf/OptionSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/URLHash.h>
#import <wtf/UUID.h>
#import <wtf/WallTime.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringHash.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSLocale;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS NSUUID;

#define THROW_UNLESS(condition, message) \
    if (!(condition)) [[unlikely]] \
        [NSException raise:NSInternalInconsistencyException format:message]

namespace API {
class Data;
}

namespace WebKit {

template<typename T> T *filterObjects(T *container, bool NS_NOESCAPE (^block)(id key, id value));
template<> NSArray *filterObjects<NSArray>(NSArray *, bool NS_NOESCAPE (^block)(id key, id value));
template<> NSDictionary *filterObjects<NSDictionary>(NSDictionary *, bool NS_NOESCAPE (^block)(id key, id value));
template<> NSSet *filterObjects<NSSet>(NSSet *, bool NS_NOESCAPE (^block)(id key, id value));

template<typename T>
T *filterObjects(const RetainPtr<T>& container, bool NS_NOESCAPE (^block)(id key, id value))
{
    return filterObjects<T>(container.get(), block);
}

template<typename T> T *mapObjects(T *container, id NS_NOESCAPE (^block)(id key, id value));
template<> NSArray *mapObjects<NSArray>(NSArray *, id NS_NOESCAPE (^block)(id key, id value));
template<> NSDictionary *mapObjects<NSDictionary>(NSDictionary *, id NS_NOESCAPE (^block)(id key, id value));
template<> NSSet *mapObjects<NSSet>(NSSet *, id NS_NOESCAPE (^block)(id key, id value));

template<typename T>
T *mapObjects(const RetainPtr<T>& container, id NS_NOESCAPE (^block)(id key, id value))
{
    return mapObjects<T>(container.get(), block);
}

template<typename T>
T *objectForKey(NSDictionary *dictionary, id key, bool returningNilIfEmpty = true, Class containingObjectsOfClass = Nil)
{
    ASSERT(returningNilIfEmpty);
    ASSERT(!containingObjectsOfClass);
    // Specialized implementations in CocoaHelpers.mm handle returningNilIfEmpty and containingObjectsOfClass for different Foundation types.
    return dynamic_objc_cast<T>(dictionary[key]);
}

template<typename T>
T *objectForKey(const RetainPtr<NSDictionary>& dictionary, id key, bool returningNilIfEmpty = true, Class containingObjectsOfClass = Nil)
{
    return objectForKey<T>(dictionary.get(), key, returningNilIfEmpty, containingObjectsOfClass);
}

inline bool boolForKey(NSDictionary *dictionary, id key, bool defaultValue)
{
    NSNumber *value = dynamic_objc_cast<NSNumber>(dictionary[key]);
    return value ? value.boolValue : defaultValue;
}

template<typename T>
inline std::optional<RetainPtr<T>> toOptional(T *maybeNil)
{
    if (maybeNil)
        return maybeNil;
    return std::nullopt;
}

inline std::optional<String> toOptional(NSString *maybeNil)
{
    if (maybeNil)
        return maybeNil;
    return std::nullopt;
}

inline CocoaImage *toCocoaImage(RefPtr<WebCore::Icon> icon)
{
    return icon ? icon->image() : nil;
}

enum class JSONOptions {
    FragmentsAllowed = 1 << 0, /// Allows for top-level scalar types, in addition to arrays and dictionaries.
};

using JSONOptionSet = OptionSet<JSONOptions>;

bool isValidJSONObject(id, JSONOptionSet = { });

id parseJSON(NSString *, JSONOptionSet = { }, NSError ** = nullptr);
id parseJSON(NSData *, JSONOptionSet = { }, NSError ** = nullptr);
id parseJSON(API::Data&, JSONOptionSet = { }, NSError ** = nullptr);

NSString *encodeJSONString(id, JSONOptionSet = { }, NSError ** = nullptr);
NSData *encodeJSONData(id, JSONOptionSet = { }, NSError ** = nullptr);

NSDictionary *dictionaryWithLowercaseKeys(NSDictionary *);
NSDictionary *dictionaryWithKeys(NSDictionary *, NSArray *keys);
NSDictionary *mergeDictionaries(NSDictionary *, NSDictionary *);
NSDictionary *mergeDictionariesAndSetValues(NSDictionary *, NSDictionary *);

NSString *privacyPreservingDescription(NSError *);

NSURL *ensureDirectoryExists(NSURL *directory);

NSString *escapeCharactersInString(NSString *, NSString *charactersToEscape);

void callAfterRandomDelay(Function<void()>&&);

NSDate *toAPI(const WallTime&);
WallTime toImpl(NSDate *);

NSSet *toAPI(const HashSet<URL>&);

NSSet *toAPI(const HashSet<String>&);
NSArray *toAPIArray(const HashSet<String>&);
HashSet<String> toImpl(NSSet *);

using DataMap = HashMap<String, Variant<String, Ref<API::Data>>>;
DataMap toDataMap(NSDictionary *);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
