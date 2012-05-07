/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 University of Szeged. All rights reserved.
 *
 * GNU Lesser General Public License Usage
 * This file may be used under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 * Please review the following information to ensure the GNU Lesser
 * General Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU General
 * Public License version 3.0 as published by the Free Software Foundation.
 * Please review the following information to ensure the GNU General Public
 * License version 3.0 requirements will be met:
 * http://www.gnu.org/copyleft/gpl.html.
 *
 */

#include "TestFontDatabase.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/private/qcore_mac_p.h>
#include <QtPlatformSupport/private/qfontengine_coretext_p.h>
#import <Foundation/Foundation.h>

// From Qt.
static const char *languageForWritingSystem[] = {
    0,     // Any
    "en",  // Latin
    "el",  // Greek
    "ru",  // Cyrillic
    "hy",  // Armenian
    "he",  // Hebrew
    "ar",  // Arabic
    "syr", // Syriac
    "div", // Thaana
    "hi",  // Devanagari
    "bn",  // Bengali
    "pa",  // Gurmukhi
    "gu",  // Gujarati
    "or",  // Oriya
    "ta",  // Tamil
    "te",  // Telugu
    "kn",  // Kannada
    "ml",  // Malayalam
    "si",  // Sinhala
    "th",  // Thai
    "lo",  // Lao
    "bo",  // Tibetan
    "my",  // Myanmar
    "ka",  // Georgian
    "km",  // Khmer
    "zh-cn", // SimplifiedChinese
    "zh-tw", // TraditionalChinese
    "ja",  // Japanese
    "ko",  // Korean
    "vi",  // Vietnamese
    0, // Symbol
    0, // Ogham
    0, // Runic
    0 // N'Ko
};
enum { LanguageCount = sizeof(languageForWritingSystem) / sizeof(const char*) };

QFont TestFontDatabase::defaultFont() const
{
    return QFont(QStringLiteral("Nimbus Sans L"));
}

void TestFontDatabase::populateFontDatabase()
{
    // Determine the set of default system fonts in order to filter them out.
    QCFType<CTFontCollectionRef> systemCollection(CTFontCollectionCreateFromAvailableFonts(0));
    QCFType<CFArrayRef> systemFonts = systemCollection ? CTFontCollectionCreateMatchingFontDescriptors(systemCollection) : 0;
    if (!systemFonts)
        qFatal("Cannot get system fonts\n");

    QByteArray testFontsVar = getenv("WEBKIT_TESTFONTS");
    QDir fontsDir(QString::fromLatin1(testFontsVar));
    if (testFontsVar.isEmpty() || !fontsDir.exists()) {
        qFatal("\n\n"
                "----------------------------------------------------------------------\n"
                "QCoreTextFontDatabaseWKTest:\n"
                "WEBKIT_TESTFONTS environment variable is not set correctly.\n"
                "This variable has to point to the directory containing the fonts\n"
                "you can clone from git://gitorious.org/qtwebkit/testfonts.git\n"
                "----------------------------------------------------------------------\n"
               );
    }

    QCFType<CFMutableSetRef> systemFontSet(CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks));
    for (int i = 0; i < CFArrayGetCount(systemFonts); ++i) {
        CTFontDescriptorRef font = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(systemFonts, i));
        CFSetAddValue(systemFontSet, font);
    }

    // Add the test fonts to the set.
    QStringList filters;
    filters << QStringLiteral("*.ttf");
    QFileInfoList files = fontsDir.entryInfoList(filters, QDir::Files);
    foreach (QFileInfo file, files) {
        QByteArray path = QFile::encodeName(file.canonicalFilePath());
        QCFType<CFURLRef> fontFile(CFURLCreateFromFileSystemRepresentation(0, reinterpret_cast<const UInt8*>(path.constData()), path.length(), false));
        CFErrorRef error;
        if (!CTFontManagerRegisterFontsForURL(fontFile, kCTFontManagerScopeProcess, &error)) {
            CFRelease(error);
            qFatal("Failed registering font file: %s\n", path.constData());
        }
    }

    // This substitution is done via fonts.conf on Linux.
    QFont::insertSubstitution(QStringLiteral("Helvetica"), QString::fromLatin1(defaultWebKitTestFontFamily));

    // ----------------------------------------------------------------------------
    // This is a copy of QCoretextFontDatabase::populateFontDatabase from Qt except
    // we filter out the system fonts.

    QCFType<CTFontCollectionRef> collection = CTFontCollectionCreateFromAvailableFonts(0);
    QCFType<CFArrayRef> fonts = CTFontCollectionCreateMatchingFontDescriptors(collection);

    QString foundryName = QLatin1String("CoreText");
    const int numFonts = CFArrayGetCount(fonts);
    QHash<QString, QString> psNameToFamily;
    for (int i = 0; i < numFonts; ++i) {
        CTFontDescriptorRef font = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fonts, i));
        if (CFSetGetCountOfValue(systemFontSet, font))
            continue;
        QCFString familyName = static_cast<CFStringRef>(CTFontDescriptorCopyLocalizedAttribute(font, kCTFontFamilyNameAttribute, NULL));
        QCFType<CFDictionaryRef> styles = static_cast<CFDictionaryRef>(CTFontDescriptorCopyAttribute(font, kCTFontTraitsAttribute));
        QFont::Weight weight = QFont::Normal;
        QFont::Style style = QFont::StyleNormal;
        QFont::Stretch stretch = QFont::Unstretched;
        bool fixedPitch = false;

        if (styles) {
            if (CFNumberRef weightValue = static_cast<CFNumberRef>(CFDictionaryGetValue(styles, kCTFontWeightTrait))) {
                Q_ASSERT(CFNumberIsFloatType(weightValue));
                double d;
                if (CFNumberGetValue(weightValue, kCFNumberDoubleType, &d))
                    weight = (d > 0.0) ? QFont::Bold : QFont::Normal;
            }
            if (CFNumberRef italic = static_cast<CFNumberRef>(CFDictionaryGetValue(styles, kCTFontSlantTrait))) {
                Q_ASSERT(CFNumberIsFloatType(italic));
                double d;
                if (CFNumberGetValue(italic, kCFNumberDoubleType, &d)) {
                    if (d > 0.0)
                        style = QFont::StyleItalic;
                }
            }
            if (CFNumberRef symbolic = static_cast<CFNumberRef>(CFDictionaryGetValue(styles, kCTFontSymbolicTrait))) {
                int d;
                if (CFNumberGetValue(symbolic, kCFNumberSInt32Type, &d)) {
                    if (d & kCTFontMonoSpaceTrait)
                        fixedPitch = true;
                    if (d & kCTFontExpandedTrait)
                        stretch = QFont::Expanded;
                    else if (d & kCTFontCondensedTrait)
                        stretch = QFont::Condensed;
                }
            }
        }

        int pixelSize = 0;
        if (QCFType<CFNumberRef> size = static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(font, kCTFontSizeAttribute))) {
            if (CFNumberIsFloatType(size)) {
                double d;
                CFNumberGetValue(size, kCFNumberDoubleType, &d);
                pixelSize = d;
            } else {
                CFNumberGetValue(size, kCFNumberIntType, &pixelSize);
            }
        }

        QSupportedWritingSystems writingSystems;
        if (QCFType<CFArrayRef> languages = static_cast<CFArrayRef>(CTFontDescriptorCopyAttribute(font, kCTFontLanguagesAttribute))) {
            CFIndex length = CFArrayGetCount(languages);
            for (int i = 1; i < LanguageCount; ++i) {
                if (!languageForWritingSystem[i])
                    continue;
                QCFString lang = CFStringCreateWithCString(NULL, languageForWritingSystem[i], kCFStringEncodingASCII);
                if (CFArrayContainsValue(languages, CFRangeMake(0, length), lang))
                    writingSystems.setSupported(QFontDatabase::WritingSystem(i));
            }
        }

        CFRetain(font);
        QPlatformFontDatabase::registerFont(familyName, foundryName, weight, style, stretch,
                                            true /* antialiased */, true /* scalable */,
                                            pixelSize, fixedPitch, writingSystems, (void *) font);
        CFStringRef psName = static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(font, kCTFontNameAttribute));
        // we need PostScript Name to family name mapping for fallback list construction
        psNameToFamily[QCFString::toQString((NSString *) psName)] = familyName;
        CFRelease(psName);
    }
    // ----------------------------------------------------------------------------
}
