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
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

typedef UncheckedKeyHashMap<String, String, ASCIICaseInsensitiveHash> DialogFeaturesMap;

static void setWindowFeature(WindowFeatures&, StringView key, StringView value);

static DialogFeaturesMap parseDialogFeaturesMap(StringView);
static std::optional<bool> boolFeature(const DialogFeaturesMap&, ASCIILiteral key);
static std::optional<float> floatFeature(const DialogFeaturesMap&, ASCIILiteral key, float min, float max);

// https://html.spec.whatwg.org/#feature-separator
static bool isSeparator(UChar character, FeatureMode mode)
{
    if (mode == FeatureMode::Viewport)
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '=' || character == ',';

    // FIXME: this should be isASCIIWhitespace
    return isUnicodeCompatibleASCIIWhitespace(character) || character == '=' || character == ',';
}

WindowFeatures parseWindowFeatures(StringView featuresString)
{
    WindowFeatures features;

    if (featuresString.isEmpty())
        return features;

    processFeaturesString(featuresString, FeatureMode::Window, [&features](StringView key, StringView value) {
        setWindowFeature(features, key, value);
    });

    return features;
}

// Window: https://html.spec.whatwg.org/#concept-window-open-features-tokenize
// Viewport: https://developer.apple.com/library/content/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html#//apple_ref/doc/uid/TP40008193-SW6
// FIXME: We should considering aligning Viewport feature parsing with Window features parsing.
void processFeaturesString(StringView features, FeatureMode mode, const Function<void(StringView type, StringView value)>& callback)
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

OptionSet<DisabledAdaptations> parseDisabledAdaptations(StringView disabledAdaptationsString)
{
    OptionSet<DisabledAdaptations> disabledAdaptations;
    for (auto name : disabledAdaptationsString.split(',')) {
        auto trimmedName = name.trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
        if (equalIgnoringASCIICase(trimmedName, watchAdaptationName()))
            disabledAdaptations.add(DisabledAdaptations::Watch);
    }
    return disabledAdaptations;
}

static void setWindowFeature(WindowFeatures& features, StringView key, StringView value)
{
    // Listing a key with no value is shorthand for key=yes
    int numericValue;
    if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "yes"_s) || equalLettersIgnoringASCIICase(value, "true"_s))
        numericValue = 1;
    else
        numericValue = parseIntegerAllowingTrailingJunk<int>(value).value_or(0);

    // We treat key of "resizable" here as an additional feature rather than setting resizeable to true.
    // This is consistent with Firefox, but could also be handled at another level.

    if (equalLettersIgnoringASCIICase(key, "left"_s) || equalLettersIgnoringASCIICase(key, "screenx"_s))
        features.x = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "top"_s) || equalLettersIgnoringASCIICase(key, "screeny"_s))
        features.y = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "width"_s) || equalLettersIgnoringASCIICase(key, "innerwidth"_s))
        features.width = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "height"_s) || equalLettersIgnoringASCIICase(key, "innerheight"_s))
        features.height = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "popup"_s))
        features.popup = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "menubar"_s))
        features.menuBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "toolbar"_s))
        features.toolBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "location"_s))
        features.locationBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "status"_s))
        features.statusBarVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "fullscreen"_s))
        features.fullscreen = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "scrollbars"_s))
        features.scrollbarsVisible = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "resizable"_s))
        features.resizable = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "noopener"_s))
        features.noopener = numericValue;
    else if (equalLettersIgnoringASCIICase(key, "noreferrer"_s))
        features.noreferrer = numericValue;
    else if (key.length() || value.length()) {
        features.hasAdditionalFeatures = true;
        if (numericValue == 1)
            features.additionalFeatures.append(key.toString());
    }
}

WindowFeatures parseDialogFeatures(StringView dialogFeaturesString, const FloatRect& screenAvailableRect)
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

    float width = floatFeature(featuresMap, "dialogwidth"_s, 100, screenAvailableRect.width()).value_or(620); // default here came from frame size of dialog in MacIE
    float height = floatFeature(featuresMap, "dialogheight"_s, 100, screenAvailableRect.height()).value_or(450); // default here came from frame size of dialog in MacIE

    features.width = width;
    features.height = height;

    features.x = floatFeature(featuresMap, "dialogleft"_s, screenAvailableRect.x(), screenAvailableRect.maxX() - width);
    features.y = floatFeature(featuresMap, "dialogtop"_s, screenAvailableRect.y(), screenAvailableRect.maxY() - height);

    if (boolFeature(featuresMap, "center"_s).value_or(true)) {
        if (!features.x)
            features.x = screenAvailableRect.x() + (screenAvailableRect.width() - width) / 2;
        if (!features.y)
            features.y = screenAvailableRect.y() + (screenAvailableRect.height() - height) / 2;
    }

    features.resizable = boolFeature(featuresMap, "resizable"_s).value_or(false);
    features.scrollbarsVisible = boolFeature(featuresMap, "scroll"_s).value_or(true);
    features.statusBarVisible = boolFeature(featuresMap, "status"_s).value_or(false);

    return features;
}

static std::optional<bool> boolFeature(const DialogFeaturesMap& features, ASCIILiteral key)
{
    auto it = features.find(key);
    if (it == features.end())
        return std::nullopt;

    auto& value = it->value;
    return value.isNull()
        || value == "1"_s
        || equalLettersIgnoringASCIICase(value, "yes"_s)
        || equalLettersIgnoringASCIICase(value, "on"_s);
}

static std::optional<float> floatFeature(const DialogFeaturesMap& features, ASCIILiteral key, float min, float max)
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

static DialogFeaturesMap parseDialogFeaturesMap(StringView string)
{
    // FIXME: Not clear why we take such a different approach to parsing dialog features
    // as opposed to window features (using a map, different parsing quirks).

    DialogFeaturesMap features;

    for (auto featureString : string.split(';')) {
        size_t separatorPosition = featureString.find('=');
        size_t colonPosition = featureString.find(':');
        if (separatorPosition != notFound && colonPosition != notFound)
            continue; // ignore strings that have both = and :
        if (separatorPosition == notFound)
            separatorPosition = colonPosition;

        auto key = featureString.left(separatorPosition).trim(isUnicodeCompatibleASCIIWhitespace<UChar>).toString();

        // Null string for value indicates key without value.
        String value;
        if (separatorPosition != notFound) {
            auto valueView = featureString.substring(separatorPosition + 1).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
            value = valueView.left(valueView.find(' ')).toString();
        }

        features.set(WTFMove(key), WTFMove(value));
    }

    return features;
}

} // namespace WebCore
