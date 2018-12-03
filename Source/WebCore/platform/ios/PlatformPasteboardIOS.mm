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
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebItemProviderPasteboard.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIColor.h>
#import <UIKit/UIImage.h>
#import <UIKit/UIPasteboard.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/ListHashSet.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/text/StringHash.h>

#define PASTEBOARD_SUPPORTS_ITEM_PROVIDERS (PLATFORM(IOS_FAMILY) && !(PLATFORM(WATCHOS) || PLATFORM(APPLETV)))
#define PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA (PASTEBOARD_SUPPORTS_ITEM_PROVIDERS && !PLATFORM(IOSMAC))
#define NSURL_SUPPORTS_TITLE (!PLATFORM(IOSMAC))

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIColor)
SOFT_LINK_CLASS(UIKit, UIImage)
SOFT_LINK_CLASS(UIKit, UIPasteboard)

namespace WebCore {

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS
PlatformPasteboard::PlatformPasteboard(const String& name)
{
    if (name == "data interaction pasteboard")
        m_pasteboard = [WebItemProviderPasteboard sharedInstance];
    else
        m_pasteboard = [getUIPasteboardClass() generalPasteboard];
}
#else
PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}
#endif

void PlatformPasteboard::getTypes(Vector<String>& types)
{
    for (NSString *pasteboardType in [m_pasteboard pasteboardTypes])
        types.append(pasteboardType);
}

void PlatformPasteboard::getTypesByFidelityForItemAtIndex(Vector<String>& types, int index)
{
    if (index >= [m_pasteboard numberOfItems] || ![m_pasteboard respondsToSelector:@selector(pasteboardTypesByFidelityForItemAtIndex:)])
        return;

    NSArray *pasteboardTypesByFidelity = [m_pasteboard pasteboardTypesByFidelityForItemAtIndex:index];
    for (NSString *typeIdentifier in pasteboardTypesByFidelity)
        types.append(typeIdentifier);
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

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS

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

Vector<PasteboardItemInfo> PlatformPasteboard::allPasteboardItemInfo()
{
    Vector<PasteboardItemInfo> itemInfo;
    for (NSInteger itemIndex = 0; itemIndex < [m_pasteboard numberOfItems]; ++itemIndex)
        itemInfo.append(informationForItemAtIndex(itemIndex));
    return itemInfo;
}

PasteboardItemInfo PlatformPasteboard::informationForItemAtIndex(int index)
{
    if (index >= [m_pasteboard numberOfItems])
        return { };

    PasteboardItemInfo info;
    if ([m_pasteboard respondsToSelector:@selector(preferredFileUploadURLAtIndex:fileType:)]) {
        NSString *fileType = nil;
        info.pathForFileUpload = [m_pasteboard preferredFileUploadURLAtIndex:index fileType:&fileType].path;
        info.contentTypeForFileUpload = fileType;
    }

    NSItemProvider *itemProvider = [[m_pasteboard itemProviders] objectAtIndex:index];
#if PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA
    info.preferredPresentationStyle = pasteboardItemPresentationStyle(itemProvider.preferredPresentationStyle);
#endif
    info.containsFileURLAndFileUploadContent = itemProvider.web_containsFileURLAndFileUploadContent;
    info.suggestedFileName = itemProvider.suggestedName;
    for (NSString *typeIdentifier in itemProvider.registeredTypeIdentifiers) {
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
        break;
    }

    return info;
}

#else

PasteboardItemInfo PlatformPasteboard::informationForItemAtIndex(int)
{
    return { };
}

Vector<PasteboardItemInfo> PlatformPasteboard::allPasteboardItemInfo()
{
    return { };
}

#endif

static bool pasteboardMayContainFilePaths(id<AbstractPasteboard> pasteboard)
{
#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS
    if ([pasteboard isKindOfClass:[WebItemProviderPasteboard class]])
        return false;
#endif

    for (NSString *type in pasteboard.pasteboardTypes) {
        if (Pasteboard::shouldTreatCocoaTypeAsFile(type))
            return true;
    }
    return false;
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
    UIColor *uiColor = [NSKeyedUnarchiver unarchivedObjectOfClass:getUIColorClass() fromData:data error:nil];
    return Color(uiColor.CGColor);
}

URL PlatformPasteboard::url()
{
    return URL();
}

long PlatformPasteboard::copy(const String&)
{
    return 0;
}

long PlatformPasteboard::addTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setBufferForType(SharedBuffer*, const String&)
{
    return 0;
}

long PlatformPasteboard::setURL(const PasteboardURL&)
{
    return 0;
}

long PlatformPasteboard::setStringForType(const String&, const String&)
{
    return 0;
}

long PlatformPasteboard::changeCount() const
{
    return [m_pasteboard changeCount];
}

String PlatformPasteboard::uniqueName()
{
    return String();
}

String PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(const String& domType)
{
    if (domType == "text/plain")
        return kUTTypePlainText;

    if (domType == "text/html")
        return kUTTypeHTML;

    if (domType == "text/uri-list")
        return kUTTypeURL;

    return { };
}

#if PASTEBOARD_SUPPORTS_ITEM_PROVIDERS

static NSString *webIOSPastePboardType = @"iOS rich content paste pasteboard type";

static void registerItemToPasteboard(WebItemProviderRegistrationInfoList *representationsToRegister, id <AbstractPasteboard> pasteboard)
{
#if PLATFORM(IOSMAC)
    // In iOSMac, -[UIPasteboard setItemProviders:] is not yet supported, so we fall back to setting an item dictionary when
    // populating the pasteboard upon copy.
    if ([pasteboard isKindOfClass:getUIPasteboardClass()]) {
        auto itemDictionary = adoptNS([[NSMutableDictionary alloc] init]);
        [representationsToRegister enumerateItems:[itemDictionary] (id <WebItemProviderRegistrar> item, NSUInteger) {
            if ([item respondsToSelector:@selector(typeIdentifierForClient)] && [item respondsToSelector:@selector(dataForClient)])
                [itemDictionary setObject:item.dataForClient forKey:item.typeIdentifierForClient];
        }];
        [pasteboard setItems:@[ itemDictionary.get() ]];
        return;
    }
#endif // PLATFORM(IOSMAC)

    if (NSItemProvider *itemProvider = representationsToRegister.itemProvider)
        [pasteboard setItemProviders:@[ itemProvider ]];
    else
        [pasteboard setItemProviders:@[ ]];

    if ([pasteboard respondsToSelector:@selector(stageRegistrationList:)])
        [pasteboard stageRegistrationList:representationsToRegister];
}

long PlatformPasteboard::setColor(const Color& color)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    UIColor *uiColor = [getUIColorClass() colorWithCGColor:cachedCGColor(color)];
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

#if !PLATFORM(IOSMAC)
    [representationsToRegister addData:[webIOSPastePboardType dataUsingEncoding:NSUTF8StringEncoding] forType:webIOSPastePboardType];
#endif

    ASSERT(content.clientTypes.size() == content.clientData.size());
    for (size_t i = 0, size = content.clientTypes.size(); i < size; ++i)
        [representationsToRegister addData:content.clientData[i]->createNSData().get() forType:content.clientTypes[i]];

    if (content.dataInWebArchiveFormat) {
        auto webArchiveData = content.dataInWebArchiveFormat->createNSData();
#if !PLATFORM(IOSMAC)
        [representationsToRegister addData:webArchiveData.get() forType:WebArchivePboardType];
#endif
        [representationsToRegister addData:webArchiveData.get() forType:(__bridge NSString *)kUTTypeWebArchive];
    }

    if (content.dataInAttributedStringFormat) {
        NSAttributedString *attributedString = unarchivedObjectOfClassesFromData([NSSet setWithObject:[NSAttributedString class]], content.dataInAttributedStringFormat->createNSData().get());
        if (attributedString)
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
    customData.origin = content.contentOrigin;
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

static const char *safeTypeForDOMToReadAndWriteForPlatformType(const String& platformType)
{
    auto cfType = platformType.createCFString();
    if (UTTypeConformsTo(cfType.get(), kUTTypePlainText))
        return "text/plain"_s;

    if (UTTypeConformsTo(cfType.get(), kUTTypeHTML) || UTTypeConformsTo(cfType.get(), (CFStringRef)WebArchivePboardType)
        || UTTypeConformsTo(cfType.get(), kUTTypeRTF) || UTTypeConformsTo(cfType.get(), kUTTypeFlatRTFD))
        return "text/html"_s;

    if (UTTypeConformsTo(cfType.get(), kUTTypeURL))
        return "text/uri-list"_s;

    return nullptr;
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

        NSDictionary *teamDataObject = unarchivedObjectOfClassesFromData([NSSet setWithObjects:[NSDictionary class], [NSString class], [NSArray class], nil], provider.teamData);
        if (!teamDataObject)
            continue;

        id originInTeamData = [teamDataObject objectForKey:@(originKeyForTeamData)];
        if (![originInTeamData isKindOfClass:[NSString class]])
            continue;
        if (String((NSString *)originInTeamData) != origin)
            continue;

        id customTypes = [(NSDictionary *)teamDataObject objectForKey:@(customTypesKeyForTeamData)];
        if (![customTypes isKindOfClass:[NSArray class]])
            continue;

        for (NSString *type in customTypes)
            domPasteboardTypes.add(type);
    }
#endif // PASTEBOARD_SUPPORTS_PRESENTATION_STYLE_AND_TEAM_DATA

    if (NSData *serializedCustomData = [m_pasteboard dataForPasteboardType:@(PasteboardCustomData::cocoaType())]) {
        auto data = PasteboardCustomData::fromSharedBuffer(SharedBuffer::create(serializedCustomData).get());
        if (data.origin == origin) {
            for (auto& type : data.orderedTypes)
                domPasteboardTypes.add(type);
        }
    }

    for (NSString *type in [m_pasteboard pasteboardTypes]) {
        if ([type isEqualToString:@(PasteboardCustomData::cocoaType())])
            continue;

        if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type)) {
            domPasteboardTypes.add(type);
            continue;
        }

        if (auto* coercedType = safeTypeForDOMToReadAndWriteForPlatformType(type)) {
            auto domTypeAsString = String::fromUTF8(coercedType);
            if (domTypeAsString == "text/uri-list") {
                BOOL ableToDetermineProtocolOfPasteboardURL = ![m_pasteboard isKindOfClass:[WebItemProviderPasteboard class]];
                if (ableToDetermineProtocolOfPasteboardURL && stringForType(kUTTypeURL).isEmpty())
                    continue;

                if ([[m_pasteboard pasteboardTypes] containsObject:(__bridge NSString *)kUTTypeFileURL])
                    continue;
            }
            domPasteboardTypes.add(WTFMove(domTypeAsString));
        }
    }

    return copyToVector(domPasteboardTypes);
}

long PlatformPasteboard::write(const PasteboardCustomData& data)
{
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    if (data.sameOriginCustomData.size()) {
        if (auto serializedSharedBuffer = data.createSharedBuffer()->createNSData()) {
            // We stash the list of supplied pasteboard types in teamData here for compatibility with drag and drop.
            // Since the contents of item providers cannot be loaded prior to drop, but the pasteboard types are
            // contained within the custom data blob and we need to vend them to the page when firing `dragover`
            // events, we need an additional in-memory representation of the pasteboard types array that contains
            // all of the custom types. We use the teamData property, available on NSItemProvider on iOS, to store
            // this information, since the contents of teamData are immediately available prior to the drop.
            NSMutableArray<NSString *> *typesAsNSArray = [NSMutableArray array];
            for (auto& type : data.orderedTypes)
                [typesAsNSArray addObject:type];
            [representationsToRegister setTeamData:securelyArchivedDataWithRootObject(@{ @(originKeyForTeamData) : data.origin, @(customTypesKeyForTeamData) : typesAsNSArray })];
            [representationsToRegister addData:serializedSharedBuffer.get() forType:@(PasteboardCustomData::cocoaType())];
        }
    }

    for (auto& type : data.orderedTypes) {
        NSString *stringValue = data.platformData.get(type);
        if (!stringValue.length)
            continue;

        auto cocoaType = platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type).createCFString();
        if (UTTypeConformsTo(cocoaType.get(), kUTTypeURL))
            [representationsToRegister addRepresentingObject:[NSURL URLWithString:stringValue]];
        else if (UTTypeConformsTo(cocoaType.get(), kUTTypePlainText))
            [representationsToRegister addRepresentingObject:stringValue];
        else
            [representationsToRegister addData:[stringValue dataUsingEncoding:NSUTF8StringEncoding] forType:(NSString *)cocoaType.get()];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
    return [m_pasteboard changeCount];
}

#else

long PlatformPasteboard::setColor(const Color&)
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

long PlatformPasteboard::write(const PasteboardCustomData&)
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

RefPtr<SharedBuffer> PlatformPasteboard::readBuffer(int index, const String& type) const
{
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];

    RetainPtr<NSArray> pasteboardItem = [m_pasteboard dataForPasteboardType:type inItemSet:indexSet];

    if (![pasteboardItem count])
        return nullptr;
    return SharedBuffer::create([pasteboardItem.get() objectAtIndex:0]);
}

String PlatformPasteboard::readString(int index, const String& type) const
{
    if (type == String(kUTTypeURL)) {
        String title;
        return [(NSURL *)readURL(index, title) absoluteString];
    }

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

URL PlatformPasteboard::readURL(int index, String& title) const
{
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

    NSMutableArray *typesArray = [NSMutableArray arrayWithCapacity:types.size()];
    for (auto type : types)
        [typesArray addObject:(NSString *)type];

    [m_pasteboard updateSupportedTypeIdentifiers:typesArray];
}

}

#endif // PLATFORM(IOS_FAMILY)
