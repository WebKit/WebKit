/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "WindowFeatures.h"

#include "FloatRect.h"
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

typedef HashMap<String, String, ASCIICaseInsensitiveHash> DialogFeaturesMap;

static void setWindowFeature(WindowFeatures&, StringView key, StringView value);

static DialogFeaturesMap parseDialogFeaturesMap(const String&);
static std::optional<bool> boolFeature(const DialogFeaturesMap&, const char* key);
static std::optional<float> floatFeature(const DialogFeaturesMap&, const char* key, float min, float max);

// https://html.spec.whatwg.org/#feature-separator
static bool isSeparator(UChar character, FeatureMode mode)
{
    if (mode == FeatureMode::Viewport)
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '=' || character == ',';

    return isASCIISpace(character) || character == '=' || character == ',';
}

WindowFeatures parseWindowFeatures(StringView featuresString)
{
    // The IE rule is: all features except for channelmode and fullscreen default to YES, but
    // if the user specifies a feature string, all features default to NO. (There is no public
    // standard that applies to this method.)
    //
    // <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
    // We always allow a window to be resized, which is consistent with Firefox.

    WindowFeatures features;

    if (featuresString.isEmpty())
        return features;

    features.menuBarVisible = false;
    features.statusBarVisible = false;
    features.toolBarVisible = false;
    features.locationBarVisible = false;
    features.scrollbarsVisible = false;
    features.noopener = false;

    processFeaturesString(featuresString, FeatureMode::Window, [&features](StringView key, StringView value) {
        setWindowFeature(features, key, value);
    });

    return features;
}

// Window: https://html.spec.whatwg.org/#concept-window-open-features-tokenize
// Viewport: https://developer.apple.com/library/content/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html#//apple_ref/doc/uid/TP40008193-SW6
// FIXME: We should considering aligning Viewport feature parsing with Window features parsing.
void processFeaturesString(StringView features, FeatureMode mode, const WTF::Function<void(StringView type, StringView value)>& callback)
{
    unsigned length = features.length();
    for (unsigned i = 0; i < length; ) {
        // Skip to first non-separator.
        while (i < length && isSeparator(features[i], mode))
            ++i;
        unsigned keyBegin = i;

        // Skip to first separator.
        while (i < length && !isSeparator(features[i], mode))
            i++;
        unsigned keyEnd = i;

        // Skip to first '=', but don't skip past a ',' or a non-separator.
        while (i < length && features[i] != '=' && features[i] != ',' && (mode == FeatureMode::Viewport || isSeparator(features[i], mode)))
            ++i;

        // Skip to first non-separator, but don't skip past a ','.
        if (mode == FeatureMode::Viewport || (i < length && isSeparator(features[i], mode))) {
            while (i < length && isSeparator(features[i], mode) && features[i] != ',')
                ++i;
            unsigned valueBegin = i;

            // Skip to first separator.
            while (i < length && !isSeparator(features[i], mode))
                ++i;
            unsigned valueEnd = i;
            callback(features.substring(keyBegin, keyEnd - keyBegin), features.substring(valueBegin, valueEnd - valueBegin));
        } else
            callback(features.substring(keyBegin, keyEnd - keyBegin), StringView());
    }
}

OptionSet<DisabledAdaptations> parseDisabledAdaptations(const String& disabledAdaptationsString)
{
    OptionSet<DisabledAdaptations> disabledAdaptations;
    for (auto& name : disabledAdaptationsString.split(',')) {
        auto normalizedName = name.stripWhiteSpace().convertToASCIILowercase();
        if (normalizedName == watchAdaptationName())
            disabledAdaptations.add(DisabledAdaptations::Watch);
    }
    return disabledAdaptations;
}

static void setWindowFeature(WindowFeatures& features, StringView key, StringView value)
{
    // Listing a key with no value is shorthand for key=yes
    int numericValue;
    if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "yes"))
        numericValue = 1;
    else
        numericValue = value.toInt();

    // We treat key of "resizable" here as an additional feature rather than setting resizeable to true.
    // This is consistent with Firefox, but could also be handled at another level.

    if (equalLettersIgnoringASCIICase(key, "left") || equalLettersIgnoringASCIICase(key, "screenx"))
        features.x = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "top") || equalLettersIgnoringASCIICase(key, "screeny"))
        features.y = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "width") || equalLettersIgnoringASCIICase(key, "innerwidth"))
        features.width = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "height") || equalLettersIgnoringASCIICase(key, "innerheight"))
        features.height = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "menubar"))
        features.menuBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "toolbar"))
        features.toolBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "location"))
        features.locationBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "status"))
        features.statusBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "fullscreen"))
        features.fullscreen = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "scrollbars"))
        features.scrollbarsVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "noopener"))
        features.noopener = true;
    else if (numericValue == 1)
        features.additionalFeatures.append(key.toString());
}

WindowFeatures parseDialogFeatures(const String& dialogFeaturesString, const FloatRect& screenAvailableRect)
{
    auto featuresMap = parseDialogFeaturesMap(dialogFeaturesString);

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    WindowFeatures features;

    features.menuBarVisible = false;
    features.toolBarVisible = false;
    features.locationBarVisible = false;
    features.dialog = true;

    float width = floatFeature(featuresMap, "dialogwidth", 100, screenAvailableRect.width()).value_or(620); // default here came from frame size of dialog in MacIE
    float height = floatFeature(featuresMap, "dialogheight", 100, screenAvailableRect.height()).value_or(450); // default here came from frame size of dialog in MacIE

    features.width = width;
    features.height = height;

    features.x = floatFeature(featuresMap, "dialogleft", screenAvailableRect.x(), screenAvailableRect.maxX() - width);
    features.y = floatFeature(featuresMap, "dialogtop", screenAvailableRect.y(), screenAvailableRect.maxY() - height);

    if (boolFeature(featuresMap, "center").value_or(true)) {
        if (!features.x)
            features.x = screenAvailableRect.x() + (screenAvailableRect.width() - width) / 2;
        if (!features.y)
            features.y = screenAvailableRect.y() + (screenAvailableRect.height() - height) / 2;
    }

    features.resizable = boolFeature(featuresMap, "resizable").value_or(false);
    features.scrollbarsVisible = boolFeature(featuresMap, "scroll").value_or(true);
    features.statusBarVisible = boolFeature(featuresMap, "status").value_or(false);

    return features;
}

static std::optional<bool> boolFeature(const DialogFeaturesMap& features, const char* key)
{
    auto it = features.find(key);
    if (it == features.end())
        return std::nullopt;

    auto& value = it->value;
    return value.isNull()
        || value == "1"
        || equalLettersIgnoringASCIICase(value, "yes")
        || equalLettersIgnoringASCIICase(value, "on");
}

static std::optional<float> floatFeature(const DialogFeaturesMap& features, const char* key, float min, float max)
{
    auto it = features.find(key);
    if (it == features.end())
        return std::nullopt;

    // FIXME: The toDouble function does not offer a way to tell "0q" from string with no digits in it: Both
    // return the number 0 and false for ok. But "0q" should yield the minimum rather than the default.
    bool ok;
    double parsedNumber = it->value.toDouble(&ok);
    if ((!parsedNumber && !ok) || std::isnan(parsedNumber))
        return std::nullopt;
    if (parsedNumber < min || max <= min)
        return min;
    if (parsedNumber > max)
        return max;

    // FIXME: Seems strange to cast a double to int and then convert back to a float. Why is this a good idea?
    return static_cast<int>(parsedNumber);
}

static DialogFeaturesMap parseDialogFeaturesMap(const String& string)
{
    // FIXME: Not clear why we take such a different approach to parsing dialog features
    // as opposed to window features (using a map, different parsing quirks).

    DialogFeaturesMap features;

    for (auto& featureString : string.split(';')) {
        size_t separatorPosition = featureString.find('=');
        size_t colonPosition = featureString.find(':');
        if (separatorPosition != notFound && colonPosition != notFound)
            continue; // ignore strings that have both = and :
        if (separatorPosition == notFound)
            separatorPosition = colonPosition;

        String key = featureString.left(separatorPosition).stripWhiteSpace();

        // Null string for value indicates key without value.
        String value;
        if (separatorPosition != notFound) {
            value = featureString.substring(separatorPosition + 1).stripWhiteSpace();
            value = value.left(value.find(' '));
        }

        features.set(key, value);
    }

    return features;
}

} // namespace WebCore
