/*
 * Copyright (C) 2013-2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformPasteboard.h"

#if PLATFORM(IOS_FAMILY)

#import "Color.h"
#import "Image.h"
#import "Pasteboard.h"
#import "RuntimeApplicationChecks.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebItemProviderPasteboard.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIColor.h>
#import <UIKit/UIImage.h>
#import <UIKit/UIPasteboard.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/ListHashSet.h>
#import <wtf/URL.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringHash.h>

#import <pal/ios/UIKitSoftLink.h>

#define PASTEBOARD_SUPPORTS_ITEM_PROVIDERS (PLATFORM(IOS_FAMILY) && !(PLATFORM(WATCHOS) || PLATFORM(APPLETV)))
#define PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA (PASTEBOARD_SUPPORTS_ITEM_PROVIDERS && !PLATFORM(MACCATALYST))
#define NSURL_SUPPORTS_TITLE (!PLATFORM(MACCATALYST))

namespace WebCore {

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard([PAL::getUIPasteboardClass() generalPasteboard])
{
}

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS
PlatformPasteboard::PlatformPasteboard(const String& name)
{
    if (name == Pasteboard::nameOfDragPasteboard())
        m_pasteboard = [WebItemProviderPasteboard sharedInstance];
    else
        m_pasteboard = [PAL::getUIPasteboardClass() generalPasteboard];
}
#else
PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard([PAL::getUIPasteboardClass() generalPasteboard])
{
}
#endif

void PlatformPasteboard::getTypes(Vector<String>& types)
{
    types = makeVector<String>([m_pasteboard pasteboardTypes]);
}

RefPtr<SharedBuffer> PlatformPasteboard::bufferForType(const String& type)
{
    if (NSData *data = [m_pasteboard dataForPasteboardType:type])
        return SharedBuffer::create(data);
    return nullptr;
}

void PlatformPasteboard::getPathnamesForType(Vector<String>&, const String&) const
{
}

int PlatformPasteboard::numberOfFiles() const
{
    return [m_pasteboard respondsToSelector:@selector(numberOfFiles)] ? [m_pasteboard numberOfFiles] : 0;
}

static bool shouldTreatAtLeastOneTypeAsFile(NSArray<NSString *> *platformTypes)
{
    for (NSString *type in platformTypes) {
        if (Pasteboard::shouldTreatCocoaTypeAsFile(type))
            return true;
    }
    return false;
}

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS

static const char *safeTypeForDOMToReadAndWriteForPlatformType(const String& platformType, PlatformPasteboard::IncludeImageTypes includeImageTypes)
{
    auto cfType = platformType.createCFString();
    if (UTTypeConformsTo(cfType.get(), kUTTypePlainText))
        return "text/plain"_s;

    if (UTTypeConformsTo(cfType.get(), kUTTypeHTML) || UTTypeConformsTo(cfType.get(), (CFStringRef)WebArchivePboardType)
        || UTTypeConformsTo(cfType.get(), kUTTypeRTF) || UTTypeConformsTo(cfType.get(), kUTTypeFlatRTFD))
        return "text/html"_s;

    if (UTTypeConformsTo(cfType.get(), kUTTypeURL))
        return "text/uri-list"_s;

    if (includeImageTypes == PlatformPasteboard::IncludeImageTypes::Yes && UTTypeConformsTo(cfType.get(), kUTTypePNG))
        return "image/png"_s;

    return nullptr;
}

static Vector<String> webSafeTypes(NSArray<NSString *> *platformTypes, PlatformPasteboard::IncludeImageTypes includeImageTypes, Function<bool()>&& shouldAvoidExposingURLType)
{
    ListHashSet<String> domPasteboardTypes;
    for (NSString *type in platformTypes) {
        if ([type isEqualToString:@(PasteboardCustomData::cocoaType())])
            continue;

        if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type)) {
            domPasteboardTypes.add(type);
            continue;
        }

        if (auto* coercedType = safeTypeForDOMToReadAndWriteForPlatformType(type, includeImageTypes)) {
            auto domTypeAsString = String::fromUTF8(coercedType);
            if (domTypeAsString == "text/uri-list"_s && ([platformTypes containsObject:(__bridge NSString *)kUTTypeFileURL] || shouldAvoidExposingURLType()))
                continue;

            domPasteboardTypes.add(WTFMove(domTypeAsString));
        }
    }
    return copyToVector(domPasteboardTypes);
}

#if PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA

static PasteboardItemPresentationStyle pasteboardItemPresentationStyle(UIPreferredPresentationStyle style)
{
    switch (style) {
    case UIPreferredPresentationStyleUnspecified:
        return PasteboardItemPresentationStyle::Unspecified;
    case UIPreferredPresentationStyleInline:
        return PasteboardItemPresentationStyle::Inline;
    case UIPreferredPresentationStyleAttachment:
        return PasteboardItemPresentationStyle::Attachment;
    default:
        ASSERT_NOT_REACHED();
        return PasteboardItemPresentationStyle::Unspecified;
    }
}

#endif // PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA

Optional<PasteboardItemInfo> PlatformPasteboard::informationForItemAtIndex(size_t index, int64_t changeCount)
{
    if (index >= static_cast<NSUInteger>([m_pasteboard numberOfItems]))
        return WTF::nullopt;

    if (this->changeCount() != changeCount)
        return WTF::nullopt;

    PasteboardItemInfo info;
    NSItemProvider *itemProvider = [[m_pasteboard itemProviders] objectAtIndex:index];
    if ([m_pasteboard respondsToSelector:@selector(fileUploadURLsAtIndex:fileTypes:)]) {
        NSArray<NSString *> *fileTypes = nil;
        NSArray *urls = [m_pasteboard fileUploadURLsAtIndex:index fileTypes:&fileTypes];
        ASSERT(fileTypes.count == urls.count);

        info.pathsForFileUpload.reserveInitialCapacity(urls.count);
        for (NSURL *url in urls)
            info.pathsForFileUpload.uncheckedAppend(url.path);

        info.platformTypesForFileUpload.reserveInitialCapacity(fileTypes.count);
        for (NSString *fileType in fileTypes)
            info.platformTypesForFileUpload.uncheckedAppend(fileType);
    } else {
        NSArray *fileTypes = itemProvider.web_fileUploadContentTypes;
        info.platformTypesForFileUpload.reserveInitialCapacity(fileTypes.count);
        info.pathsForFileUpload.reserveInitialCapacity(fileTypes.count);
        for (NSString *fileType in fileTypes) {
            info.platformTypesForFileUpload.uncheckedAppend(fileType);
            info.pathsForFileUpload.uncheckedAppend({ });
        }
    }

#if PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA
    info.preferredPresentationStyle = pasteboardItemPresentationStyle(itemProvider.preferredPresentationStyle);
#endif
    if (!CGSizeEqualToSize(itemProvider.preferredPresentationSize, CGSizeZero)) {
        auto adjustedPreferredPresentationHeight = [](auto height) -> Optional<double> {
            if (!IOSApplication::isMobileMail() && !IOSApplication::isMailCompositionService())
                return { height };
            // Mail's max-width: 100%; default style is in conflict with the preferred presentation size and can lead to unexpectedly stretched images. Not setting the height forces layout to preserve the aspect ratio.
            return { };
        };
        info.preferredPresentationSize = PresentationSize { itemProvider.preferredPresentationSize.width, adjustedPreferredPresentationHeight(itemProvider.preferredPresentationSize.height) };
    }
    info.containsFileURLAndFileUploadContent = itemProvider.web_containsFileURLAndFileUploadContent;
    info.suggestedFileName = itemProvider.suggestedName;
    NSArray<NSString *> *registeredTypeIdentifiers = itemProvider.registeredTypeIdentifiers;
    info.platformTypesByFidelity.reserveInitialCapacity(registeredTypeIdentifiers.count);
    for (NSString *typeIdentifier in registeredTypeIdentifiers) {
        info.platformTypesByFidelity.uncheckedAppend(typeIdentifier);
        CFStringRef cfTypeIdentifier = (CFStringRef)typeIdentifier;
        if (!UTTypeIsDeclared(cfTypeIdentifier))
            continue;

        if (UTTypeConformsTo(cfTypeIdentifier, kUTTypeText))
            continue;

        if (UTTypeConformsTo(cfTypeIdentifier, kUTTypeURL))
            continue;

        if (UTTypeConformsTo(cfTypeIdentifier, kUTTypeRTFD))
            continue;

        if (UTTypeConformsTo(cfTypeIdentifier, kUTTypeFlatRTFD))
            continue;

        info.isNonTextType = true;
    }

    info.webSafeTypesByFidelity = webSafeTypes(registeredTypeIdentifiers, IncludeImageTypes::Yes, [&] {
        return shouldTreatAtLeastOneTypeAsFile(registeredTypeIdentifiers) && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(readString(index, kUTTypeURL));
    });

    return info;
}

#else

Optional<PasteboardItemInfo> PlatformPasteboard::informationForItemAtIndex(size_t, int64_t)
{
    return WTF::nullopt;
}

#endif

static bool pasteboardMayContainFilePaths(id<AbstractPasteboard> pasteboard)
{
#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS
    if ([pasteboard isKindOfClass:[WebItemProviderPasteboard class]])
        return false;
#endif
    return shouldTreatAtLeastOneTypeAsFile(pasteboard.pasteboardTypes);
}

String PlatformPasteboard::stringForType(const String& type) const
{
    auto result = readString(0, type);

    if (pasteboardMayContainFilePaths(m_pasteboard.get()) && type == String { kUTTypeURL }) {
        if (!Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(result))
            result = { };
    }

    return result;
}

Color PlatformPasteboard::color()
{
    NSData *data = [m_pasteboard dataForPasteboardType:UIColorPboardType];
    UIColor *uiColor = [NSKeyedUnarchiver unarchivedObjectOfClass:PAL::getUIColorClass() fromData:data error:nil];
    return Color(uiColor.CGColor);
}

URL PlatformPasteboard::url()
{
    return URL();
}

int64_t PlatformPasteboard::copy(const String&)
{
    return 0;
}

int64_t PlatformPasteboard::addTypes(const Vector<String>&)
{
    return 0;
}

int64_t PlatformPasteboard::setTypes(const Vector<String>&)
{
    return 0;
}

int64_t PlatformPasteboard::setBufferForType(SharedBuffer*, const String&)
{
    return 0;
}

int64_t PlatformPasteboard::setURL(const PasteboardURL&)
{
    return 0;
}

int64_t PlatformPasteboard::setStringForType(const String&, const String&)
{
    return 0;
}

int64_t PlatformPasteboard::changeCount() const
{
    return [m_pasteboard changeCount];
}

String PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(const String& domType, IncludeImageTypes includeImageTypes)
{
    if (domType == "text/plain")
        return kUTTypePlainText;

    if (domType == "text/html")
        return kUTTypeHTML;

    if (domType == "text/uri-list")
        return kUTTypeURL;

    if (includeImageTypes == IncludeImageTypes::Yes && domType == "image/png")
        return kUTTypePNG;

    return { };
}

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS

static NSString *webIOSPastePboardType = @"iOS rich content paste pasteboard type";

static void registerItemsToPasteboard(NSArray<WebItemProviderRegistrationInfoList *> *itemLists, id <AbstractPasteboard> pasteboard)
{
#if PLATFORM(MACCATALYST)
    // In macCatalyst, -[UIPasteboard setItemProviders:] is not yet supported, so we fall back to setting an item dictionary when
    // populating the pasteboard upon copy.
    if ([pasteboard isKindOfClass:PAL::getUIPasteboardClass()]) {
        auto itemDictionaries = adoptNS([[NSMutableArray alloc] initWithCapacity:itemLists.count]);
        for (WebItemProviderRegistrationInfoList *representationsToRegister in itemLists) {
            auto itemDictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:representationsToRegister.numberOfItems]);
            [representationsToRegister enumerateItems:[itemDictionary] (id <WebItemProviderRegistrar> item, NSUInteger) {
                if ([item respondsToSelector:@selector(typeIdentifierForClient)] && [item respondsToSelector:@selector(dataForClient)])
                    [itemDictionary setObject:item.dataForClient forKey:item.typeIdentifierForClient];
            }];
            [itemDictionaries addObject:itemDictionary.get()];
        }
        [pasteboard setItems:itemDictionaries.get()];
        return;
    }
#endif // PLATFORM(MACCATALYST)

    auto itemProviders = adoptNS([[NSMutableArray alloc] initWithCapacity:itemLists.count]);
    for (WebItemProviderRegistrationInfoList *representationsToRegister in itemLists) {
        if (auto *itemProvider = representationsToRegister.itemProvider)
            [itemProviders addObject:itemProvider];
    }

    [pasteboard setItemProviders:itemProviders.get()];

    if ([pasteboard respondsToSelector:@selector(stageRegistrationLists:)])
        [pasteboard stageRegistrationLists:itemLists];
}

static void registerItemToPasteboard(WebItemProviderRegistrationInfoList *representationsToRegister, id <AbstractPasteboard> pasteboard)
{
    registerItemsToPasteboard(@[ representationsToRegister ], pasteboard);
}

int64_t PlatformPasteboard::setColor(const Color& color)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    UIColor *uiColor = [PAL::getUIColorClass() colorWithCGColor:cachedCGColor(color)];
    [representationsToRegister addData:[NSKeyedArchiver archivedDataWithRootObject:uiColor requiringSecureCoding:NO error:nil] forType:UIColorPboardType];
    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
    return 0;
}

static void addRepresentationsForPlainText(WebItemProviderRegistrationInfoList *itemsToRegister, const String& plainText)
{
    if (plainText.isEmpty())
        return;

    NSURL *platformURL = [NSURL URLWithString:plainText];
    if (URL(platformURL).isValid())
        [itemsToRegister addRepresentingObject:platformURL];

    [itemsToRegister addData:[(NSString *)plainText dataUsingEncoding:NSUTF8StringEncoding] forType:(NSString *)kUTTypeUTF8PlainText];
}

bool PlatformPasteboard::allowReadingURLAtIndex(const URL& url, int index) const
{
    NSItemProvider *itemProvider = (NSUInteger)index < [m_pasteboard itemProviders].count ? [[m_pasteboard itemProviders] objectAtIndex:index] : nil;
    for (NSString *type in itemProvider.registeredTypeIdentifiers) {
        if (UTTypeConformsTo((CFStringRef)type, kUTTypeURL))
            return true;
    }

    return url.isValid();
}

void PlatformPasteboard::write(const PasteboardWebContent& content)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);

#if !PLATFORM(MACCATALYST)
    [representationsToRegister addData:[webIOSPastePboardType dataUsingEncoding:NSUTF8StringEncoding] forType:webIOSPastePboardType];
#endif

    ASSERT(content.clientTypes.size() == content.clientData.size());
    for (size_t i = 0, size = content.clientTypes.size(); i < size; ++i)
        [representationsToRegister addData:content.clientData[i]->createNSData().get() forType:content.clientTypes[i]];

    if (content.dataInWebArchiveFormat) {
        auto webArchiveData = content.dataInWebArchiveFormat->createNSData();
#if PLATFORM(MACCATALYST)
        NSString *webArchiveType = (__bridge NSString *)kUTTypeWebArchive;
#else
        // FIXME: We should additionally register "com.apple.webarchive" once <rdar://problem/46830277> is fixed.
        NSString *webArchiveType = WebArchivePboardType;
#endif
        [representationsToRegister addData:webArchiveData.get() forType:webArchiveType];
    }

    if (content.dataInAttributedStringFormat) {
        if (NSAttributedString *attributedString = [NSKeyedUnarchiver unarchivedObjectOfClasses:[NSSet setWithObject:NSAttributedString.class] fromData:content.dataInAttributedStringFormat->createNSData().get() error:nullptr])
            [representationsToRegister addRepresentingObject:attributedString];
    }

    if (content.dataInRTFDFormat)
        [representationsToRegister addData:content.dataInRTFDFormat->createNSData().get() forType:(NSString *)kUTTypeFlatRTFD];

    if (content.dataInRTFFormat)
        [representationsToRegister addData:content.dataInRTFFormat->createNSData().get() forType:(NSString *)kUTTypeRTF];

    if (!content.dataInHTMLFormat.isEmpty()) {
        NSData *htmlAsData = [(NSString *)content.dataInHTMLFormat dataUsingEncoding:NSUTF8StringEncoding];
        [representationsToRegister addData:htmlAsData forType:(NSString *)kUTTypeHTML];
    }

    if (!content.dataInStringFormat.isEmpty())
        addRepresentationsForPlainText(representationsToRegister.get(), content.dataInStringFormat);

    PasteboardCustomData customData;
    customData.setOrigin(content.contentOrigin);
    [representationsToRegister addData:customData.createSharedBuffer()->createNSData().get() forType:@(PasteboardCustomData::cocoaType())];

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
}

void PlatformPasteboard::write(const PasteboardImage& pasteboardImage)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);

    auto& types = pasteboardImage.clientTypes;
    auto& data = pasteboardImage.clientData;
    ASSERT(types.size() == data.size());
    for (size_t i = 0, size = types.size(); i < size; ++i)
        [representationsToRegister addData:data[i]->createNSData().get() forType:types[i]];

    if (pasteboardImage.resourceData && !pasteboardImage.resourceMIMEType.isEmpty()) {
        auto utiOrMIMEType = pasteboardImage.resourceMIMEType;
        if (!isDeclaredUTI(utiOrMIMEType))
            utiOrMIMEType = UTIFromMIMEType(utiOrMIMEType);

        auto imageData = pasteboardImage.resourceData->createNSData();
        [representationsToRegister addData:imageData.get() forType:(NSString *)utiOrMIMEType];
        [representationsToRegister setPreferredPresentationSize:pasteboardImage.imageSize];
        [representationsToRegister setSuggestedName:pasteboardImage.suggestedName];
    }

    // FIXME: When writing a PasteboardImage, we currently always place the image data at a higer fidelity than the
    // associated image URL. However, in the case of an image enclosed by an anchor, we might want to consider the
    // the URL (i.e. the anchor's href attribute) to be a higher fidelity representation.
    auto& pasteboardURL = pasteboardImage.url;
    if (NSURL *nsURL = pasteboardURL.url) {
#if NSURL_SUPPORTS_TITLE
        nsURL._title = pasteboardURL.title.isEmpty() ? WTF::userVisibleString(pasteboardURL.url) : (NSString *)pasteboardURL.title;
#endif
        [representationsToRegister addRepresentingObject:nsURL];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
}

void PlatformPasteboard::write(const String& pasteboardType, const String& text)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    NSString *pasteboardTypeAsNSString = pasteboardType;
    if (!text.isEmpty() && pasteboardTypeAsNSString.length) {
        auto pasteboardTypeAsCFString = (CFStringRef)pasteboardTypeAsNSString;
        if (UTTypeConformsTo(pasteboardTypeAsCFString, kUTTypeURL) || UTTypeConformsTo(pasteboardTypeAsCFString, kUTTypeText))
            addRepresentationsForPlainText(representationsToRegister.get(), text);
        else
            [representationsToRegister addData:[pasteboardTypeAsNSString dataUsingEncoding:NSUTF8StringEncoding] forType:pasteboardType];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
}

void PlatformPasteboard::write(const PasteboardURL& url)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    if (NSURL *nsURL = url.url) {
#if NSURL_SUPPORTS_TITLE
        if (!url.title.isEmpty())
            nsURL._title = url.title;
#endif
        [representationsToRegister addRepresentingObject:nsURL];
        [representationsToRegister addRepresentingObject:(NSString *)url.url.string()];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
}

static const char originKeyForTeamData[] = "com.apple.WebKit.drag-and-drop-team-data.origin";
static const char customTypesKeyForTeamData[] = "com.apple.WebKit.drag-and-drop-team-data.custom-types";

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String& origin) const
{
    ListHashSet<String> domPasteboardTypes;
#if PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA
    for (NSItemProvider *provider in [m_pasteboard itemProviders]) {
        if (!provider.teamData.length)
            continue;

        NSSet *classes = [NSSet setWithObjects:NSDictionary.class, NSString.class, NSArray.class, nil];
        id teamDataObject = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:provider.teamData error:nullptr];
        if (![teamDataObject isKindOfClass:NSDictionary.class])
            continue;

        id originInTeamData = [teamDataObject objectForKey:@(originKeyForTeamData)];
        if (![originInTeamData isKindOfClass:NSString.class])
            continue;
        if (String((NSString *)originInTeamData) != origin)
            continue;

        id customTypes = [teamDataObject objectForKey:@(customTypesKeyForTeamData)];
        if (![customTypes isKindOfClass:NSArray.class])
            continue;

        for (NSString *type in customTypes)
            domPasteboardTypes.add(type);
    }
#endif // PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA

    if (NSData *serializedCustomData = [m_pasteboard dataForPasteboardType:@(PasteboardCustomData::cocoaType())]) {
        auto data = PasteboardCustomData::fromSharedBuffer(SharedBuffer::create(serializedCustomData).get());
        if (data.origin() == origin) {
            for (auto& type : data.orderedTypes())
                domPasteboardTypes.add(type);
        }
    }

    auto webSafePasteboardTypes = webSafeTypes([m_pasteboard pasteboardTypes], IncludeImageTypes::No, [&] {
        BOOL ableToDetermineProtocolOfPasteboardURL = ![m_pasteboard isKindOfClass:[WebItemProviderPasteboard class]];
        return ableToDetermineProtocolOfPasteboardURL && stringForType(kUTTypeURL).isEmpty();
    });

    for (auto& type : webSafePasteboardTypes)
        domPasteboardTypes.add(type);

    return copyToVector(domPasteboardTypes);
}

static RetainPtr<WebItemProviderRegistrationInfoList> createItemProviderRegistrationList(const PasteboardCustomData& data)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    if (data.hasSameOriginCustomData() || !data.origin().isEmpty()) {
        if (auto serializedSharedBuffer = data.createSharedBuffer()->createNSData()) {
            // We stash the list of supplied pasteboard types in teamData here for compatibility with drag and drop.
            // Since the contents of item providers cannot be loaded prior to drop, but the pasteboard types are
            // contained within the custom data blob and we need to vend them to the page when firing `dragover`
            // events, we need an additional in-memory representation of the pasteboard types array that contains
            // all of the custom types. We use the teamData property, available on NSItemProvider on iOS, to store
            // this information, since the contents of teamData are immediately available prior to the drop.
            NSDictionary *teamDataDictionary = @{ @(originKeyForTeamData) : data.origin(), @(customTypesKeyForTeamData) : createNSArray(data.orderedTypes()).get() };
            if (NSData *teamData = [NSKeyedArchiver archivedDataWithRootObject:teamDataDictionary requiringSecureCoding:YES error:nullptr]) {
                [representationsToRegister setTeamData:teamData];
                [representationsToRegister addData:serializedSharedBuffer.get() forType:@(PasteboardCustomData::cocoaType())];
            }
        }
    }

    data.forEachPlatformStringOrBuffer([&] (auto& type, auto& value) {
        auto cocoaType = PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type, PlatformPasteboard::IncludeImageTypes::Yes);
        if (cocoaType.isEmpty())
            return;

        if (WTF::holds_alternative<String>(value)) {
            if (WTF::get<String>(value).isNull())
                return;

            NSString *nsStringValue = WTF::get<String>(value);
            auto cfType = cocoaType.createCFString();
            if (UTTypeConformsTo(cfType.get(), kUTTypeURL))
                [representationsToRegister addRepresentingObject:[NSURL URLWithString:nsStringValue]];
            else if (UTTypeConformsTo(cfType.get(), kUTTypePlainText))
                [representationsToRegister addRepresentingObject:nsStringValue];
            else
                [representationsToRegister addData:[nsStringValue dataUsingEncoding:NSUTF8StringEncoding] forType:(NSString *)cocoaType];
            return;
        }

        auto buffer = WTF::get<Ref<SharedBuffer>>(value);
        [representationsToRegister addData:buffer->createNSData().get() forType:(NSString *)cocoaType];
    });

    return representationsToRegister;
}

int64_t PlatformPasteboard::write(const Vector<PasteboardCustomData>& itemData)
{
    auto registrationLists = createNSArray(itemData, [] (auto& data) {
        return createItemProviderRegistrationList(data);
    });
    registerItemsToPasteboard(registrationLists.get(), m_pasteboard.get());
    return [m_pasteboard changeCount];
}

#else

int64_t PlatformPasteboard::setColor(const Color&)
{
    return 0;
}

bool PlatformPasteboard::allowReadingURLAtIndex(const URL&, int) const
{
    return false;
}

void PlatformPasteboard::write(const PasteboardWebContent&)
{
}

void PlatformPasteboard::write(const PasteboardImage&)
{
}

void PlatformPasteboard::write(const String&, const String&)
{
}

void PlatformPasteboard::write(const PasteboardURL&)
{
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String&) const
{
    return { };
}

int64_t PlatformPasteboard::write(const Vector<PasteboardCustomData>&)
{
    return 0;
}

#endif

int PlatformPasteboard::count() const
{
    return [m_pasteboard numberOfItems];
}

Vector<String> PlatformPasteboard::allStringsForType(const String& type) const
{
    auto numberOfItems = count();
    Vector<String> strings;
    strings.reserveInitialCapacity(numberOfItems);
    for (int index = 0; index < numberOfItems; ++index) {
        String value = readString(index, type);
        if (!value.isEmpty())
            strings.uncheckedAppend(WTFMove(value));
    }
    return strings;
}

RefPtr<SharedBuffer> PlatformPasteboard::readBuffer(size_t index, const String& type) const
{
    if ((NSInteger)index < 0 || (NSInteger)index >= [m_pasteboard numberOfItems])
        return nullptr;

    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];

    RetainPtr<NSArray> pasteboardItem = [m_pasteboard dataForPasteboardType:type inItemSet:indexSet];

    if (![pasteboardItem count])
        return nullptr;
    return SharedBuffer::create([pasteboardItem.get() objectAtIndex:0]);
}

String PlatformPasteboard::readString(size_t index, const String& type) const
{
    if (type == String(kUTTypeURL)) {
        String title;
        return [(NSURL *)readURL(index, title) absoluteString];
    }

    if ((NSInteger)index < 0 || (NSInteger)index >= [m_pasteboard numberOfItems])
        return { };

    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];
    auto value = retainPtr([m_pasteboard valuesForPasteboardType:type inItemSet:indexSet].firstObject ?: [m_pasteboard dataForPasteboardType:type inItemSet:indexSet].firstObject);
    if (!value)
        return { };

    if ([value isKindOfClass:[NSData class]])
        value = adoptNS([[NSString alloc] initWithData:(NSData *)value.get() encoding:NSUTF8StringEncoding]);
    
    if (type == String(kUTTypePlainText) || type == String(kUTTypeHTML)) {
        ASSERT([value isKindOfClass:[NSString class]]);
        return [value isKindOfClass:[NSString class]] ? value.get() : nil;
    }
    if (type == String(kUTTypeText)) {
        ASSERT([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSAttributedString class]]);
        if ([value isKindOfClass:[NSString class]])
            return value.get();
        if ([value isKindOfClass:[NSAttributedString class]])
            return [(NSAttributedString *)value string];
    }

    return String();
}

URL PlatformPasteboard::readURL(size_t index, String& title) const
{
    if ((NSInteger)index < 0 || (NSInteger)index >= [m_pasteboard numberOfItems])
        return { };

    id value = [m_pasteboard valuesForPasteboardType:(__bridge NSString *)kUTTypeURL inItemSet:[NSIndexSet indexSetWithIndex:index]].firstObject;
    if (!value)
        return { };

    ASSERT([value isKindOfClass:[NSURL class]]);
    if (![value isKindOfClass:[NSURL class]])
        return { };

    NSURL *url = (NSURL *)value;
    if (!allowReadingURLAtIndex(url, index))
        return { };

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS && NSURL_SUPPORTS_TITLE
    title = [url _title];
#else
    UNUSED_PARAM(title);
#endif

    return url;
}

void PlatformPasteboard::updateSupportedTypeIdentifiers(const Vector<String>& types)
{
    if (![m_pasteboard respondsToSelector:@selector(updateSupportedTypeIdentifiers:)])
        return;

    [m_pasteboard updateSupportedTypeIdentifiers:createNSArray(types).get()];
}

int64_t PlatformPasteboard::write(const PasteboardCustomData& data)
{
    return write(Vector<PasteboardCustomData> { data });
}

bool PlatformPasteboard::containsURLStringSuitableForLoading()
{
    Vector<String> types;
    getTypes(types);
    if (!types.contains(String(kUTTypeURL)))
        return false;

    auto urlString = stringForType(kUTTypeURL);
    if (urlString.isEmpty()) {
        // On iOS, we don't get access to the contents of NSItemProviders until we perform the drag operation.
        // Thus, we consider DragData to contain an URL if it contains the `public.url` UTI type. Later down the
        // road, when we perform the drag operation, we can then check if the URL's protocol is http or https,
        // and if it isn't, we bail out of page navigation.
        return true;
    }
    return URL { [NSURL URLWithString:urlString] }.protocolIsInHTTPFamily();
}

}

#endif // PLATFORM(IOS_FAMILY)
