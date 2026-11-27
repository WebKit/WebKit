/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Sanitizer.h"

#include "HTMLNames.h"
#include "JSHTMLBodyElement.h"
#include "JSHTMLElement.h"
#include "JSSVGElement.h"
#include "MathMLNames.h"
#include "Namespace.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/Borrow.h>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Sanitizer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ParserSanitizerConfiguration);

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizer-name-canonicalize
static SanitizerCanonicalName canonicalizeName(const String& name, const String& namespaceURI)
{
    AtomString canonicalNamespace { namespaceURI };
    if (canonicalNamespace.isEmpty())
        canonicalNamespace = nullAtom();
    return { AtomString { name }, canonicalNamespace };
}

static SanitizerCanonicalName canonicalizeAttribute(const SanitizerAttribute& attribute)
{
    return WTF::switchOn(attribute, [](const String& name) {
        return SanitizerCanonicalName { AtomString { name }, nullAtom() };
    }, [](const SanitizerAttributeNamespace& dictionary) {
        return canonicalizeName(dictionary.name, dictionary.namespaceURI);
    });
}

static SanitizerCanonicalName canonicalizeElementName(const SanitizerElement& element)
{
    return WTF::switchOn(element, [](const String& name) {
        return SanitizerCanonicalName { AtomString { name }, HTMLNames::xhtmlNamespaceURI };
    }, [](const SanitizerElementNamespace& dictionary) {
        return canonicalizeName(dictionary.name, dictionary.namespaceURI);
    });
}

static Vector<SanitizerCanonicalName> canonicalizeAttributeList(const Vector<SanitizerAttribute>& list)
{
    return list.map([](auto& item) {
        return canonicalizeAttribute(item);
    });
}

static Vector<SanitizerCanonicalName> canonicalizeElementList(const Vector<SanitizerElement>& list)
{
    return list.map([](auto& item) {
        return canonicalizeElementName(item);
    });
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerelementnamespacewithattributes-canonicalize
static SanitizerCanonicalElement canonicalizeElementWithAttributes(const SanitizerElementWithAttributes& element)
{
    SanitizerCanonicalElement result = WTF::switchOn(element, [](const String& name) {
        return SanitizerCanonicalElement { { AtomString { name }, HTMLNames::xhtmlNamespaceURI }, std::nullopt, std::nullopt };
    }, [](const SanitizerElementNamespaceWithAttributes& dictionary) {
        SanitizerCanonicalElement result { canonicalizeName(dictionary.name, dictionary.namespaceURI), std::nullopt, std::nullopt };
        if (dictionary.attributes)
            result.attributes = canonicalizeAttributeList(*dictionary.attributes);
        if (dictionary.removeAttributes)
            result.removeAttributes = canonicalizeAttributeList(*dictionary.removeAttributes);
        return result;
    });
    if (!result.attributes && !result.removeAttributes)
        result.removeAttributes = Vector<SanitizerCanonicalName> { };
    return result;
}

static AtomString canonicalizeProcessingInstruction(const SanitizerPI& pi)
{
    return WTF::switchOn(pi, [](const String& target) {
        return AtomString { target };
    }, [](const SanitizerProcessingInstruction& dictionary) {
        return AtomString { dictionary.target };
    });
}

static Vector<AtomString> canonicalizeProcessingInstructionList(const Vector<SanitizerPI>& list)
{
    return list.map([](auto& item) {
        return canonicalizeProcessingInstruction(item);
    });
}

static bool isCustomDataAttribute(const SanitizerCanonicalName& attribute)
{
    return attribute.namespaceURI.isNull() && attribute.name.startsWith("data-"_s);
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#contains-a-javascript-url
static bool containsJavaScriptURL(const String& value)
{
    URL url { URL { }, value };
    return url.isValid() && url.protocolIsJavaScript();
}

static bool containsName(const Vector<SanitizerCanonicalName>& list, const SanitizerCanonicalName& name)
{
    return list.contains(name);
}

static size_t findIndexOfElement(const Vector<SanitizerCanonicalElement>& list, const SanitizerCanonicalName& name)
{
    return list.findIf([&](auto& item) {
        return item.name == name;
    });
}

// "Removing an item" from a potentially absent list; absent lists never contain anything.
static bool removeName(std::optional<Vector<SanitizerCanonicalName>>& list, const SanitizerCanonicalName& name)
{
    if (!list)
        return false;
    return list->removeFirst(name);
}

static void makeSetInPlace(Vector<SanitizerCanonicalName>& list)
{
    Vector<SanitizerCanonicalName> result;
    for (auto& item : borrow(list).get()) {
        if (!result.contains(item))
            result.append(item);
    }
    list = WTF::move(result);
}

static void removeAllInPlace(Vector<SanitizerCanonicalName>& list, const Vector<SanitizerCanonicalName>& toRemove)
{
    list.removeAllMatching([&](auto& item) {
        return toRemove.contains(item);
    });
}

static void intersectInPlace(Vector<SanitizerCanonicalName>& list, const Vector<SanitizerCanonicalName>& other)
{
    list.removeAllMatching([&](auto& item) {
        return !other.contains(item);
    });
}

static bool intersects(const Vector<SanitizerCanonicalName>& listA, const Vector<SanitizerCanonicalName>& listB)
{
    for (auto& item : borrow(listA).get()) {
        if (listB.contains(item))
            return true;
    }
    return false;
}

static bool isSubsetOf(const Vector<SanitizerCanonicalName>& subset, const Vector<SanitizerCanonicalName>& superset)
{
    for (auto& item : borrow(subset).get()) {
        if (!superset.contains(item))
            return false;
    }
    return true;
}

template<typename T> static bool hasDuplicates(const Vector<T>& list)
{
    for (size_t i = 0; i < list.size(); ++i) {
        for (size_t j = i + 1; j < list.size(); ++j) {
            if (list[i] == list[j])
                return true;
        }
    }
    return false;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-non-replaceable-elements-list
static bool isNonReplaceableElement(const SanitizerCanonicalName& name)
{
    if (name.namespaceURI == HTMLNames::xhtmlNamespaceURI)
        return name.name == "html"_s;
    if (name.namespaceURI == SVGNames::svgNamespaceURI)
        return name.name == "svg"_s;
    if (name.namespaceURI == MathMLNames::mathmlNamespaceURI)
        return name.name == "math"_s;
    return false;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-canonicalize
static SanitizerCanonicalConfig canonicalize(const SanitizerConfig& configuration, bool permissiveDefaults)
{
    SanitizerCanonicalConfig config;

    if (configuration.elements) {
        config.elements = configuration.elements->map([](auto& element) {
            return canonicalizeElementWithAttributes(element);
        });
    } else if (!configuration.removeElements)
        config.removeElements = Vector<SanitizerCanonicalName> { };

    if (configuration.removeElements)
        config.removeElements = canonicalizeElementList(*configuration.removeElements);

    if (configuration.replaceWithChildrenElements)
        config.replaceWithChildrenElements = canonicalizeElementList(*configuration.replaceWithChildrenElements);

    if (configuration.attributes)
        config.attributes = canonicalizeAttributeList(*configuration.attributes);
    else if (!configuration.removeAttributes)
        config.removeAttributes = Vector<SanitizerCanonicalName> { };

    if (configuration.removeAttributes)
        config.removeAttributes = canonicalizeAttributeList(*configuration.removeAttributes);

    if (configuration.processingInstructions)
        config.processingInstructions = canonicalizeProcessingInstructionList(*configuration.processingInstructions);
    else if (!configuration.removeProcessingInstructions) {
        if (permissiveDefaults)
            config.removeProcessingInstructions = Vector<AtomString> { };
        else
            config.processingInstructions = Vector<AtomString> { };
    }

    if (configuration.removeProcessingInstructions)
        config.removeProcessingInstructions = canonicalizeProcessingInstructionList(*configuration.removeProcessingInstructions);

    config.comments = configuration.comments.value_or(permissiveDefaults);

    if (configuration.dataAttributes)
        config.dataAttributes = *configuration.dataAttributes;
    else if (config.attributes)
        config.dataAttributes = permissiveDefaults;

    config.javascriptURLs = configuration.javascriptURLs.value_or(permissiveDefaults);

    return config;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-valid
static bool isValidConfiguration(const SanitizerCanonicalConfig& config)
{
    ASSERT(config.elements || config.removeElements);
    if (config.elements && config.removeElements)
        return false;

    ASSERT(config.processingInstructions || config.removeProcessingInstructions);
    if (config.processingInstructions && config.removeProcessingInstructions)
        return false;

    ASSERT(config.attributes || config.removeAttributes);
    if (config.attributes && config.removeAttributes)
        return false;

    if (config.elements) {
        auto names = config.elements->map([](auto& element) {
            return element.name;
        });
        if (hasDuplicates(names))
            return false;
    } else if (hasDuplicates(*config.removeElements))
        return false;

    if (config.replaceWithChildrenElements && hasDuplicates(*config.replaceWithChildrenElements))
        return false;

    if (config.processingInstructions) {
        if (hasDuplicates(*config.processingInstructions))
            return false;
    } else if (hasDuplicates(*config.removeProcessingInstructions))
        return false;

    if (config.attributes) {
        if (hasDuplicates(*config.attributes))
            return false;
    } else if (hasDuplicates(*config.removeAttributes))
        return false;

    if (config.replaceWithChildrenElements) {
        for (auto& element : borrow(*config.replaceWithChildrenElements).get()) {
            if (isNonReplaceableElement(element))
                return false;
        }
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (containsName(*config.replaceWithChildrenElements, element.name))
                    return false;
            }
        } else if (intersects(*config.removeElements, *config.replaceWithChildrenElements))
            return false;
    }

    if (config.attributes) {
        ASSERT(config.dataAttributes);
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (element.attributes && hasDuplicates(*element.attributes))
                    return false;
                if (element.removeAttributes && hasDuplicates(*element.removeAttributes))
                    return false;
                if (element.attributes && intersects(*config.attributes, *element.attributes))
                    return false;
                if (element.removeAttributes && !isSubsetOf(*element.removeAttributes, *config.attributes))
                    return false;
                if (config.dataAttributes.value_or(false) && element.attributes) {
                    for (auto& attribute : borrow(*element.attributes).get()) {
                        if (isCustomDataAttribute(attribute))
                            return false;
                    }
                }
            }
        }
        if (config.dataAttributes.value_or(false)) {
            for (auto& attribute : borrow(*config.attributes).get()) {
                if (isCustomDataAttribute(attribute))
                    return false;
            }
        }
    } else {
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (element.attributes && element.removeAttributes)
                    return false;
                if (element.attributes && hasDuplicates(*element.attributes))
                    return false;
                if (element.removeAttributes && hasDuplicates(*element.removeAttributes))
                    return false;
                if (element.attributes && intersects(*config.removeAttributes, *element.attributes))
                    return false;
                if (element.removeAttributes && intersects(*config.removeAttributes, *element.removeAttributes))
                    return false;
            }
        }
        if (config.dataAttributes)
            return false;
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-safe-default-configuration
static const SanitizerCanonicalConfig& builtinSafeDefaultConfiguration()
{
    static NeverDestroyed<SanitizerCanonicalConfig> config = [] {
        struct ElementData {
            Namespace namespaceID;
            ASCIILiteral name;
            ASCIILiteral attributes;
        };
        static constexpr auto elementData = WTF::toArray<ElementData>({
    { Namespace::MathML, "a"_s, "href hreflang type"_s },
    { Namespace::MathML, "math"_s, ""_s },
    { Namespace::MathML, "merror"_s, ""_s },
    { Namespace::MathML, "mfrac"_s, ""_s },
    { Namespace::MathML, "mi"_s, ""_s },
    { Namespace::MathML, "mmultiscripts"_s, ""_s },
    { Namespace::MathML, "mn"_s, ""_s },
    { Namespace::MathML, "mo"_s, "fence form largeop lspace maxsize minsize movablelimits rspace separator stretchy symmetric"_s },
    { Namespace::MathML, "mover"_s, "accent"_s },
    { Namespace::MathML, "mpadded"_s, "depth height lspace voffset width"_s },
    { Namespace::MathML, "mphantom"_s, ""_s },
    { Namespace::MathML, "mprescripts"_s, ""_s },
    { Namespace::MathML, "mroot"_s, ""_s },
    { Namespace::MathML, "mrow"_s, ""_s },
    { Namespace::MathML, "ms"_s, ""_s },
    { Namespace::MathML, "mspace"_s, "depth height width"_s },
    { Namespace::MathML, "msqrt"_s, ""_s },
    { Namespace::MathML, "mstyle"_s, ""_s },
    { Namespace::MathML, "msub"_s, ""_s },
    { Namespace::MathML, "msubsup"_s, ""_s },
    { Namespace::MathML, "msup"_s, ""_s },
    { Namespace::MathML, "mtable"_s, ""_s },
    { Namespace::MathML, "mtd"_s, "columnspan rowspan"_s },
    { Namespace::MathML, "mtext"_s, ""_s },
    { Namespace::MathML, "mtr"_s, ""_s },
    { Namespace::MathML, "munder"_s, "accentunder"_s },
    { Namespace::MathML, "munderover"_s, "accent accentunder"_s },
    { Namespace::MathML, "semantics"_s, ""_s },
    { Namespace::HTML, "a"_s, "href hreflang type"_s },
    { Namespace::HTML, "abbr"_s, ""_s },
    { Namespace::HTML, "address"_s, ""_s },
    { Namespace::HTML, "article"_s, ""_s },
    { Namespace::HTML, "aside"_s, ""_s },
    { Namespace::HTML, "b"_s, ""_s },
    { Namespace::HTML, "bdi"_s, ""_s },
    { Namespace::HTML, "bdo"_s, ""_s },
    { Namespace::HTML, "blockquote"_s, "cite"_s },
    { Namespace::HTML, "body"_s, ""_s },
    { Namespace::HTML, "br"_s, ""_s },
    { Namespace::HTML, "caption"_s, ""_s },
    { Namespace::HTML, "cite"_s, ""_s },
    { Namespace::HTML, "code"_s, ""_s },
    { Namespace::HTML, "col"_s, "span"_s },
    { Namespace::HTML, "colgroup"_s, "span"_s },
    { Namespace::HTML, "data"_s, "value"_s },
    { Namespace::HTML, "dd"_s, ""_s },
    { Namespace::HTML, "del"_s, "cite datetime"_s },
    { Namespace::HTML, "dfn"_s, ""_s },
    { Namespace::HTML, "div"_s, ""_s },
    { Namespace::HTML, "dl"_s, ""_s },
    { Namespace::HTML, "dt"_s, ""_s },
    { Namespace::HTML, "em"_s, ""_s },
    { Namespace::HTML, "figcaption"_s, ""_s },
    { Namespace::HTML, "figure"_s, ""_s },
    { Namespace::HTML, "footer"_s, ""_s },
    { Namespace::HTML, "h1"_s, ""_s },
    { Namespace::HTML, "h2"_s, ""_s },
    { Namespace::HTML, "h3"_s, ""_s },
    { Namespace::HTML, "h4"_s, ""_s },
    { Namespace::HTML, "h5"_s, ""_s },
    { Namespace::HTML, "h6"_s, ""_s },
    { Namespace::HTML, "head"_s, ""_s },
    { Namespace::HTML, "header"_s, ""_s },
    { Namespace::HTML, "hgroup"_s, ""_s },
    { Namespace::HTML, "hr"_s, ""_s },
    { Namespace::HTML, "html"_s, ""_s },
    { Namespace::HTML, "i"_s, ""_s },
    { Namespace::HTML, "ins"_s, "cite datetime"_s },
    { Namespace::HTML, "kbd"_s, ""_s },
    { Namespace::HTML, "li"_s, "value"_s },
    { Namespace::HTML, "main"_s, ""_s },
    { Namespace::HTML, "mark"_s, ""_s },
    { Namespace::HTML, "menu"_s, ""_s },
    { Namespace::HTML, "nav"_s, ""_s },
    { Namespace::HTML, "ol"_s, "reversed start type"_s },
    { Namespace::HTML, "p"_s, ""_s },
    { Namespace::HTML, "pre"_s, ""_s },
    { Namespace::HTML, "q"_s, ""_s },
    { Namespace::HTML, "rp"_s, ""_s },
    { Namespace::HTML, "rt"_s, ""_s },
    { Namespace::HTML, "ruby"_s, ""_s },
    { Namespace::HTML, "s"_s, ""_s },
    { Namespace::HTML, "samp"_s, ""_s },
    { Namespace::HTML, "search"_s, ""_s },
    { Namespace::HTML, "section"_s, ""_s },
    { Namespace::HTML, "small"_s, ""_s },
    { Namespace::HTML, "span"_s, ""_s },
    { Namespace::HTML, "strong"_s, ""_s },
    { Namespace::HTML, "sub"_s, ""_s },
    { Namespace::HTML, "sup"_s, ""_s },
    { Namespace::HTML, "table"_s, ""_s },
    { Namespace::HTML, "tbody"_s, ""_s },
    { Namespace::HTML, "td"_s, "colspan headers rowspan"_s },
    { Namespace::HTML, "tfoot"_s, ""_s },
    { Namespace::HTML, "th"_s, "abbr colspan headers rowspan scope"_s },
    { Namespace::HTML, "thead"_s, ""_s },
    { Namespace::HTML, "time"_s, "datetime"_s },
    { Namespace::HTML, "title"_s, ""_s },
    { Namespace::HTML, "tr"_s, ""_s },
    { Namespace::HTML, "u"_s, ""_s },
    { Namespace::HTML, "ul"_s, ""_s },
    { Namespace::HTML, "var"_s, ""_s },
    { Namespace::HTML, "wbr"_s, ""_s },
    { Namespace::SVG, "a"_s, "href hreflang type"_s },
    { Namespace::SVG, "circle"_s, "cx cy pathLength r"_s },
    { Namespace::SVG, "defs"_s, ""_s },
    { Namespace::SVG, "desc"_s, ""_s },
    { Namespace::SVG, "ellipse"_s, "cx cy pathLength rx ry"_s },
    { Namespace::SVG, "foreignObject"_s, "height width x y"_s },
    { Namespace::SVG, "g"_s, ""_s },
    { Namespace::SVG, "line"_s, "pathLength x1 x2 y1 y2"_s },
    { Namespace::SVG, "marker"_s, "markerHeight markerUnits markerWidth orient preserveAspectRatio refX refY viewBox"_s },
    { Namespace::SVG, "metadata"_s, ""_s },
    { Namespace::SVG, "path"_s, "d pathLength"_s },
    { Namespace::SVG, "polygon"_s, "pathLength points"_s },
    { Namespace::SVG, "polyline"_s, "pathLength points"_s },
    { Namespace::SVG, "rect"_s, "height pathLength rx ry width x y"_s },
    { Namespace::SVG, "svg"_s, "height preserveAspectRatio viewBox width x y"_s },
    { Namespace::SVG, "text"_s, "dx dy lengthAdjust rotate textLength x y"_s },
    { Namespace::SVG, "textPath"_s, "lengthAdjust method path side spacing startOffset textLength"_s },
    { Namespace::SVG, "title"_s, ""_s },
    { Namespace::SVG, "tspan"_s, "dx dy lengthAdjust rotate textLength x y"_s },
        });
        static constexpr std::array attributeData {
    "alignment-baseline"_s, "baseline-shift"_s, "clip-path"_s, "clip-rule"_s, "color"_s, "color-interpolation"_s,
    "cursor"_s, "dir"_s, "direction"_s, "display"_s, "displaystyle"_s, "dominant-baseline"_s,
    "fill"_s, "fill-opacity"_s, "fill-rule"_s, "font-family"_s, "font-size"_s, "font-size-adjust"_s,
    "font-stretch"_s, "font-style"_s, "font-variant"_s, "font-weight"_s, "lang"_s, "letter-spacing"_s,
    "marker-end"_s, "marker-mid"_s, "marker-start"_s, "mathbackground"_s, "mathcolor"_s, "mathsize"_s,
    "opacity"_s, "paint-order"_s, "pointer-events"_s, "scriptlevel"_s, "shape-rendering"_s, "stop-color"_s,
    "stop-opacity"_s, "stroke"_s, "stroke-dasharray"_s, "stroke-dashoffset"_s, "stroke-linecap"_s, "stroke-linejoin"_s,
    "stroke-miterlimit"_s, "stroke-opacity"_s, "stroke-width"_s, "text-anchor"_s, "text-decoration"_s, "text-overflow"_s,
    "text-rendering"_s, "title"_s, "transform"_s, "transform-origin"_s, "unicode-bidi"_s, "vector-effect"_s,
    "visibility"_s, "white-space"_s, "word-spacing"_s, "writing-mode"_s,
        };

        SanitizerCanonicalConfig config;
        config.elements = Vector<SanitizerCanonicalElement> { };
        for (auto& element : elementData) {
            const AtomString& namespaceURI = [&] -> const AtomString& {
                switch (element.namespaceID) {
                case Namespace::HTML:
                    return HTMLNames::xhtmlNamespaceURI;
                case Namespace::MathML:
                    return MathMLNames::mathmlNamespaceURI;
                case Namespace::SVG:
                    return SVGNames::svgNamespaceURI;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }();
            Vector<SanitizerCanonicalName> attributes;
            for (auto attributeName : StringView { element.attributes }.split(' '))
                attributes.append({ attributeName.toAtomString(), nullAtom() });
            config.elements->append({ { AtomString { element.name }, namespaceURI }, WTF::move(attributes), std::nullopt });
        }
        config.attributes = Vector<SanitizerCanonicalName> { };
        for (auto attributeName : attributeData)
            config.attributes->append({ AtomString { attributeName }, nullAtom() });
        config.processingInstructions = Vector<AtomString> { };
        config.comments = false;
        config.dataAttributes = false;
        config.javascriptURLs = false;
        ASSERT(isValidConfiguration(config));
        return config;
    }();
    return config;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-safe-baseline-configuration
static const Vector<SanitizerCanonicalName>& baselineRemoveElements()
{
    static NeverDestroyed<Vector<SanitizerCanonicalName>> elements = [] {
        Vector<SanitizerCanonicalName> elements;
        for (auto name : { "base"_s, "embed"_s, "frame"_s, "iframe"_s, "object"_s, "script"_s })
            elements.append({ AtomString { name }, HTMLNames::xhtmlNamespaceURI });
        for (auto name : { "script"_s, "use"_s })
            elements.append({ AtomString { name }, SVGNames::svgNamespaceURI });
        return elements;
    }();
    return elements;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-remove-element
static bool removeElementFromConfiguration(SanitizerCanonicalConfig& config, const SanitizerCanonicalName& element)
{
    bool modified = removeName(config.replaceWithChildrenElements, element);
    if (config.elements) {
        auto index = findIndexOfElement(*config.elements, element);
        if (index == notFound)
            return modified;
        config.elements->removeAt(index);
        return true;
    }
    if (containsName(*config.removeElements, element))
        return modified;
    config.removeElements->append(element);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-remove-attribute
static bool removeAttributeFromConfiguration(SanitizerCanonicalConfig& config, const SanitizerCanonicalName& attribute)
{
    if (config.attributes) {
        bool modified = config.attributes->removeFirst(attribute);
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (element.attributes && element.attributes->removeFirst(attribute))
                    modified = true;
                if (element.removeAttributes && element.removeAttributes->removeFirst(attribute))
                    ASSERT(modified);
            }
        }
        return modified;
    }
    if (containsName(*config.removeAttributes, attribute))
        return false;
    if (config.elements) {
        for (auto& element : borrow(*config.elements).get()) {
            if (element.attributes)
                element.attributes->removeFirst(attribute);
            if (element.removeAttributes)
                element.removeAttributes->removeFirst(attribute);
        }
    }
    config.removeAttributes->append(attribute);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-remove-unsafe
bool Sanitizer::removeUnsafe(SanitizerCanonicalConfig& config)
{
    ASSERT(isValidConfiguration(config));
    bool result = false;

    for (auto& element : borrow(baselineRemoveElements()).get()) {
        if (removeElementFromConfiguration(config, element))
            result = true;
    }

    Function<void(const AtomString&, const AtomString&)> removeEventHandlerAttribute = [&](const AtomString& attributeName, const AtomString&) {
        if (removeAttributeFromConfiguration(config, { attributeName, nullAtom() }))
            result = true;
    };
    JSHTMLElement::forEachEventHandlerContentAttribute(removeEventHandlerAttribute);
    JSHTMLBodyElement::forEachWindowEventHandlerContentAttribute(removeEventHandlerAttribute);
    JSSVGElement::forEachEventHandlerContentAttribute(removeEventHandlerAttribute);

    if (config.javascriptURLs) {
        result = true;
        config.javascriptURLs = false;
    }
    return result;
}

SanitizerCanonicalConfig Sanitizer::defaultConfiguration()
{
    return builtinSafeDefaultConfiguration();
}

ExceptionOr<RefPtr<ParserSanitizerConfiguration>> Sanitizer::parserConfigurationFromOptions(std::optional<SanitizerBinding>&& binding, bool safe)
{
    SanitizerCanonicalConfig configuration;
    if (!binding) {
        if (!safe)
            return RefPtr<ParserSanitizerConfiguration> { };
        configuration = defaultConfiguration();
    } else {
        auto result = WTF::switchOn(WTF::move(*binding),
            [](Ref<Sanitizer>&& sanitizer) -> ExceptionOr<SanitizerCanonicalConfig> {
                return SanitizerCanonicalConfig { sanitizer->configuration() };
            },
            [safe](SanitizerConfig&& config) -> ExceptionOr<SanitizerCanonicalConfig> {
                return canonicalizeAndValidate(config, !safe);
            },
            [](SanitizerPresets) -> ExceptionOr<SanitizerCanonicalConfig> {
                return defaultConfiguration();
            });
        if (result.hasException())
            return result.releaseException();
        configuration = result.releaseReturnValue();
    }
    if (safe)
        removeUnsafe(configuration);
    return RefPtr<ParserSanitizerConfiguration> { ParserSanitizerConfiguration::create(configuration) };
}

ExceptionOr<SanitizerCanonicalConfig> Sanitizer::canonicalizeAndValidate(const SanitizerConfig& configuration, bool permissiveDefaults)
{
    auto config = canonicalize(configuration, permissiveDefaults);
    if (!isValidConfiguration(config))
        return Exception { ExceptionCode::TypeError, "The SanitizerConfig is invalid"_s };
    return config;
}

Sanitizer::Sanitizer(SanitizerCanonicalConfig&& configuration)
    : m_configuration(WTF::move(configuration))
{
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-sanitizer
ExceptionOr<Ref<Sanitizer>> Sanitizer::create(std::optional<Variant<SanitizerConfig, SanitizerPresets>>&& configuration)
{
    if (!configuration || std::holds_alternative<SanitizerPresets>(*configuration))
        return adoptRef(*new Sanitizer(defaultConfiguration()));
    auto canonical = canonicalizeAndValidate(std::get<SanitizerConfig>(*configuration), true);
    if (canonical.hasException())
        return canonical.releaseException();
    return adoptRef(*new Sanitizer(canonical.releaseReturnValue()));
}

Ref<Sanitizer> Sanitizer::create(SanitizerCanonicalConfig&& configuration)
{
    ASSERT(isValidConfiguration(configuration));
    return adoptRef(*new Sanitizer(WTF::move(configuration)));
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizer-items-compare
static bool compareSanitizerItems(const SanitizerCanonicalName& itemA, const SanitizerCanonicalName& itemB)
{
    if (itemA.namespaceURI.isNull()) {
        if (!itemB.namespaceURI.isNull())
            return true;
    } else {
        if (itemB.namespaceURI.isNull())
            return false;
        auto namespaceOrder = codePointCompare(StringView { itemA.namespaceURI.string() }, StringView { itemB.namespaceURI.string() });
        if (namespaceOrder < 0)
            return true;
        if (namespaceOrder > 0)
            return false;
    }
    return codePointCompare(StringView { itemA.name.string() }, StringView { itemB.name.string() }) < 0;
}

static String toJSNamespace(const AtomString& namespaceURI)
{
    return namespaceURI.isNull() ? String() : namespaceURI.string();
}

static Vector<SanitizerAttribute> attributeListToJS(Vector<SanitizerCanonicalName> list)
{
    std::ranges::sort(list, compareSanitizerItems);
    return list.map([](auto& item) -> SanitizerAttribute {
        return SanitizerAttributeNamespace { item.name.string(), toJSNamespace(item.namespaceURI) };
    });
}

static Vector<SanitizerElement> elementListToJS(Vector<SanitizerCanonicalName> list)
{
    std::ranges::sort(list, compareSanitizerItems);
    return list.map([](auto& item) -> SanitizerElement {
        return SanitizerElementNamespace { item.name.string(), toJSNamespace(item.namespaceURI) };
    });
}

static Vector<SanitizerPI> processingInstructionListToJS(Vector<AtomString> list)
{
    std::ranges::sort(list, [](auto& targetA, auto& targetB) {
        return codePointCompare(StringView { targetA.string() }, StringView { targetB.string() }) < 0;
    });
    return list.map([](auto& target) -> SanitizerPI {
        return SanitizerProcessingInstruction { target.string() };
    });
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-get
SanitizerConfig Sanitizer::get() const
{
    ASSERT(isValidConfiguration(m_configuration));
    SanitizerConfig result;

    if (m_configuration.elements) {
        auto elements = *m_configuration.elements;
        std::ranges::sort(elements, [](auto& elementA, auto& elementB) {
            return compareSanitizerItems(elementA.name, elementB.name);
        });
        result.elements = elements.map([](auto& element) -> SanitizerElementWithAttributes {
            SanitizerElementNamespaceWithAttributes dictionary;
            dictionary.name = element.name.name.string();
            dictionary.namespaceURI = toJSNamespace(element.name.namespaceURI);
            if (element.attributes)
                dictionary.attributes = attributeListToJS(*element.attributes);
            if (element.removeAttributes)
                dictionary.removeAttributes = attributeListToJS(*element.removeAttributes);
            return dictionary;
        });
    } else
        result.removeElements = elementListToJS(*m_configuration.removeElements);

    if (m_configuration.replaceWithChildrenElements)
        result.replaceWithChildrenElements = elementListToJS(*m_configuration.replaceWithChildrenElements);

    if (m_configuration.processingInstructions)
        result.processingInstructions = processingInstructionListToJS(*m_configuration.processingInstructions);
    else
        result.removeProcessingInstructions = processingInstructionListToJS(*m_configuration.removeProcessingInstructions);

    if (m_configuration.attributes)
        result.attributes = attributeListToJS(*m_configuration.attributes);
    else
        result.removeAttributes = attributeListToJS(*m_configuration.removeAttributes);

    result.comments = m_configuration.comments;
    if (m_configuration.dataAttributes)
        result.dataAttributes = *m_configuration.dataAttributes;
    result.javascriptURLs = m_configuration.javascriptURLs;

    return result;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-allowelement
bool Sanitizer::allowElement(SanitizerElementWithAttributes&& element)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    auto canonical = canonicalizeElementWithAttributes(element);

    if (config.elements) {
        bool modified = removeName(config.replaceWithChildrenElements, canonical.name);

        if (config.attributes) {
            if (canonical.attributes) {
                makeSetInPlace(*canonical.attributes);
                removeAllInPlace(*canonical.attributes, *config.attributes);
                if (config.dataAttributes.value_or(false))
                    canonical.attributes->removeAllMatching(isCustomDataAttribute);
            }
            if (canonical.removeAttributes) {
                makeSetInPlace(*canonical.removeAttributes);
                intersectInPlace(*canonical.removeAttributes, *config.attributes);
            }
        } else {
            if (canonical.attributes) {
                makeSetInPlace(*canonical.attributes);
                if (canonical.removeAttributes)
                    removeAllInPlace(*canonical.attributes, *canonical.removeAttributes);
                canonical.removeAttributes = std::nullopt;
                removeAllInPlace(*canonical.attributes, *config.removeAttributes);
            }
            if (canonical.removeAttributes) {
                makeSetInPlace(*canonical.removeAttributes);
                removeAllInPlace(*canonical.removeAttributes, *config.removeAttributes);
            }
        }

        auto index = findIndexOfElement(*config.elements, canonical.name);
        if (index == notFound) {
            config.elements->append(WTF::move(canonical));
            return true;
        }
        if ((*config.elements)[index] == canonical)
            return modified;
        config.elements->removeAt(index);
        config.elements->append(WTF::move(canonical));
        return true;
    }

    if (canonical.attributes || (canonical.removeAttributes && !canonical.removeAttributes->isEmpty()))
        return false;

    bool modified = removeName(config.replaceWithChildrenElements, canonical.name);
    if (!containsName(*config.removeElements, canonical.name))
        return modified;
    config.removeElements->removeFirst(canonical.name);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-removeelement
bool Sanitizer::removeElement(SanitizerElement&& element)
{
    ASSERT(isValidConfiguration(m_configuration));
    return removeElementFromConfiguration(m_configuration, canonicalizeElementName(element));
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-replaceelementwithchildren
bool Sanitizer::replaceElementWithChildren(SanitizerElement&& element)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    auto canonical = canonicalizeElementName(element);

    if (isNonReplaceableElement(canonical))
        return false;

    bool modified = false;
    if (config.elements) {
        auto index = findIndexOfElement(*config.elements, canonical);
        if (index != notFound) {
            config.elements->removeAt(index);
            modified = true;
        }
    }
    if (removeName(config.removeElements, canonical))
        modified = true;

    if (!config.replaceWithChildrenElements)
        config.replaceWithChildrenElements = Vector<SanitizerCanonicalName> { };
    if (!containsName(*config.replaceWithChildrenElements, canonical)) {
        config.replaceWithChildrenElements->append(canonical);
        return true;
    }
    return modified;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-allowprocessinginstruction
bool Sanitizer::allowProcessingInstruction(SanitizerPI&& pi)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    auto canonical = canonicalizeProcessingInstruction(pi);

    if (config.processingInstructions) {
        if (config.processingInstructions->contains(canonical))
            return false;
        config.processingInstructions->append(canonical);
        return true;
    }

    if (!config.removeProcessingInstructions->contains(canonical))
        return false;
    config.removeProcessingInstructions->removeFirst(canonical);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-removeprocessinginstruction
bool Sanitizer::removeProcessingInstruction(SanitizerPI&& pi)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    auto canonical = canonicalizeProcessingInstruction(pi);

    if (config.processingInstructions) {
        if (!config.processingInstructions->contains(canonical))
            return false;
        config.processingInstructions->removeFirst(canonical);
        return true;
    }

    if (config.removeProcessingInstructions->contains(canonical))
        return false;
    config.removeProcessingInstructions->append(canonical);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-allowattribute
bool Sanitizer::allowAttribute(SanitizerAttribute&& attribute)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    auto canonical = canonicalizeAttribute(attribute);

    if (config.attributes) {
        if (config.dataAttributes.value_or(false) && isCustomDataAttribute(canonical))
            return false;
        if (containsName(*config.attributes, canonical))
            return false;
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (element.attributes)
                    element.attributes->removeFirst(canonical);
            }
        }
        config.attributes->append(canonical);
        return true;
    }

    if (!containsName(*config.removeAttributes, canonical))
        return false;
    config.removeAttributes->removeFirst(canonical);
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-removeattribute
bool Sanitizer::removeAttribute(SanitizerAttribute&& attribute)
{
    ASSERT(isValidConfiguration(m_configuration));
    return removeAttributeFromConfiguration(m_configuration, canonicalizeAttribute(attribute));
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-setcomments
bool Sanitizer::setComments(bool allow)
{
    if (m_configuration.comments == allow)
        return false;
    m_configuration.comments = allow;
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-setdataattributes
bool Sanitizer::setDataAttributes(bool allow)
{
    auto& config = m_configuration;
    ASSERT(isValidConfiguration(config));
    if (!config.attributes)
        return false;
    if (config.dataAttributes && *config.dataAttributes == allow)
        return false;

    if (allow) {
        if (config.elements) {
            for (auto& element : borrow(*config.elements).get()) {
                if (element.attributes)
                    element.attributes->removeAllMatching(isCustomDataAttribute);
            }
        }
        config.attributes->removeAllMatching(isCustomDataAttribute);
    }
    config.dataAttributes = allow;
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-setjavascripturls
bool Sanitizer::setJavascriptURLs(bool allow)
{
    if (m_configuration.javascriptURLs == allow)
        return false;
    m_configuration.javascriptURLs = allow;
    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-sanitizer-removeunsafe
bool Sanitizer::removeUnsafe()
{
    return removeUnsafe(m_configuration);
}

// MARK: - ParserSanitizerConfiguration

static String makeLookupKey(const AtomString& name, const AtomString& namespaceURI)
{
    if (namespaceURI.isNull())
        return makeString('-', name);
    return makeString(namespaceURI.length(), ':', namespaceURI, name);
}

static HashSet<String> makeLookupSet(const Vector<SanitizerCanonicalName>& list)
{
    HashSet<String> set;
    for (auto& item : borrow(list).get())
        set.add(makeLookupKey(item.name, item.namespaceURI));
    return set;
}

Ref<ParserSanitizerConfiguration> ParserSanitizerConfiguration::create(const SanitizerCanonicalConfig& config)
{
    return adoptRef(*new ParserSanitizerConfiguration(config));
}

ParserSanitizerConfiguration::ParserSanitizerConfiguration(const SanitizerCanonicalConfig& config)
{
    if (config.elements) {
        m_elements = HashMap<String, LocalAttributeLists> { };
        for (auto& element : borrow(*config.elements).get()) {
            LocalAttributeLists lists;
            if (element.attributes)
                lists.attributes = makeLookupSet(*element.attributes);
            if (element.removeAttributes)
                lists.removeAttributes = makeLookupSet(*element.removeAttributes);
            m_elements->set(makeLookupKey(element.name.name, element.name.namespaceURI), WTF::move(lists));
        }
    }
    if (config.removeElements)
        m_removeElements = makeLookupSet(*config.removeElements);
    if (config.replaceWithChildrenElements)
        m_replaceWithChildrenElements = makeLookupSet(*config.replaceWithChildrenElements);
    if (config.attributes)
        m_attributes = makeLookupSet(*config.attributes);
    if (config.removeAttributes)
        m_removeAttributes = makeLookupSet(*config.removeAttributes);
    if (config.processingInstructions) {
        m_processingInstructions = HashSet<String> { };
        for (auto& target : borrow(*config.processingInstructions).get())
            m_processingInstructions->add(target.string());
    }
    if (config.removeProcessingInstructions) {
        for (auto& target : borrow(*config.removeProcessingInstructions).get())
            m_removeProcessingInstructions.add(target.string());
    }
    m_comments = config.comments;
    m_dataAttributes = config.dataAttributes.value_or(false);
    m_javascriptURLs = config.javascriptURLs;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#get-the-sanitizer-action-for-an-element-name
SanitizerAction ParserSanitizerConfiguration::actionForElement(const AtomString& localName, const AtomString& namespaceURI) const
{
    auto key = makeLookupKey(localName, namespaceURI);
    if (m_replaceWithChildrenElements.contains(key))
        return SanitizerAction::ReplaceWithChildren;
    if (m_elements)
        return m_elements->contains(key) ? SanitizerAction::Keep : SanitizerAction::Remove;
    if (m_removeElements.contains(key))
        return SanitizerAction::Remove;
    return SanitizerAction::Keep;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizer-config-allows-an-attribute
bool ParserSanitizerConfiguration::allowsAttribute(const AtomString& localName, const AtomString& namespaceURI, const AtomString& value, const AtomString& elementLocalName, const AtomString& elementNamespaceURI) const
{
    auto attributeKey = makeLookupKey(localName, namespaceURI);

    const LocalAttributeLists* localLists = nullptr;
    if (m_elements) {
        auto iterator = m_elements->find(makeLookupKey(elementLocalName, elementNamespaceURI));
        if (iterator != m_elements->end())
            localLists = &iterator->value;
    }

    if (localLists && localLists->removeAttributes && localLists->removeAttributes->contains(attributeKey))
        return false;
    if (m_attributes) {
        bool allowed = m_attributes->contains(attributeKey) || (localLists && localLists->attributes && localLists->attributes->contains(attributeKey));
        if (!allowed && !(m_dataAttributes && namespaceURI.isNull() && localName.startsWith("data-"_s)))
            return false;
    } else {
        if (localLists && localLists->attributes) {
            if (!localLists->attributes->contains(attributeKey))
                return false;
        } else if (m_removeAttributes.contains(attributeKey))
            return false;
    }

    if (m_javascriptURLs)
        return true;

    bool attributeHasNullNamespace = namespaceURI.isNull();
    bool elementIsHTML = elementNamespaceURI == HTMLNames::xhtmlNamespaceURI;
    bool elementIsSVG = elementNamespaceURI == SVGNames::svgNamespaceURI;

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-navigating-url-attributes-list
    bool isNavigatingURLAttribute = false;
    if (attributeHasNullNamespace && elementIsHTML) {
        isNavigatingURLAttribute = ((elementLocalName == "a"_s || elementLocalName == "area"_s) && localName == "href"_s)
            || (elementLocalName == "form"_s && localName == "action"_s)
            || ((elementLocalName == "input"_s || elementLocalName == "button"_s) && localName == "formaction"_s);
    } else if (elementIsSVG && elementLocalName == "a"_s && localName == "href"_s)
        isNavigatingURLAttribute = attributeHasNullNamespace || namespaceURI == XLinkNames::xlinkNamespaceURI;

    if (isNavigatingURLAttribute && containsJavaScriptURL(value))
        return false;

    if (elementNamespaceURI == MathMLNames::mathmlNamespaceURI && localName == "href"_s && (attributeHasNullNamespace || namespaceURI == XLinkNames::xlinkNamespaceURI) && containsJavaScriptURL(value))
        return false;

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-animating-url-attributes-list
    if (elementIsSVG && attributeHasNullNamespace && localName == "attributeName"_s
        && (elementLocalName == "animate"_s || elementLocalName == "animateTransform"_s || elementLocalName == "set"_s)
        && (value == "href"_s || value == "xlink:href"_s))
        return false;

    return true;
}

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizer-config-allows-processing-instruction-target
bool ParserSanitizerConfiguration::allowsProcessingInstructionTarget(const String& target) const
{
    if (m_processingInstructions)
        return m_processingInstructions->contains(target);
    return !m_removeProcessingInstructions.contains(target);
}

} // namespace WebCore
