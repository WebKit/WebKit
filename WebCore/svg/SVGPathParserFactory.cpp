/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGPathParserFactory.h"

#include "SVGPathBuilder.h"
#include "SVGPathByteStreamBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathParser.h"
#include "SVGPathSegListBuilder.h"
#include "SVGPathStringSource.h"

namespace WebCore {

static SVGPathBuilder* globalSVGPathBuilder()
{
    static SVGPathBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathBuilder;

    return s_builder;
}

static SVGPathSegListBuilder* globalSVGPathSegListBuilder()
{
    static SVGPathSegListBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathSegListBuilder;

    return s_builder;
}

static SVGPathByteStreamBuilder* globalSVGPathByteStreamBuilder()
{
    static SVGPathByteStreamBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathByteStreamBuilder;

    return s_builder;
}

static SVGPathParser* globalSVGPathParser()
{
    static SVGPathParser* s_parser = 0;
    if (!s_parser)
        s_parser = new SVGPathParser;

    return s_parser;
}

SVGPathParserFactory* SVGPathParserFactory::self()
{
    static SVGPathParserFactory* s_instance = 0;
    if (!s_instance)
        s_instance = new SVGPathParserFactory;

    return s_instance;
}

SVGPathParserFactory::SVGPathParserFactory()
{
}

SVGPathParserFactory::~SVGPathParserFactory()
{
}

bool SVGPathParserFactory::buildPathFromString(const String& d, Path& result)
{
    if (d.isEmpty())
        return false;

    SVGPathBuilder* builder = globalSVGPathBuilder();
    builder->setCurrentPath(&result);

    SVGPathParser* parser = globalSVGPathParser();
    parser->setCurrentConsumer(builder);

    OwnPtr<SVGPathStringSource> source = SVGPathStringSource::create(d);
    parser->setCurrentSource(source.get());

    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    parser->setCurrentConsumer(0);
    parser->setCurrentSource(0);
    builder->setCurrentPath(0);
    return ok;
}

bool SVGPathParserFactory::buildPathFromByteStream(SVGPathByteStream* stream, Path& result)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    SVGPathBuilder* builder = globalSVGPathBuilder();
    builder->setCurrentPath(&result);

    SVGPathParser* parser = globalSVGPathParser();
    parser->setCurrentConsumer(builder);

    OwnPtr<SVGPathByteStreamSource> source = SVGPathByteStreamSource::create(stream);
    parser->setCurrentSource(source.get());

    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    parser->setCurrentConsumer(0);
    parser->setCurrentSource(0);
    builder->setCurrentPath(0);
    return ok;
}

bool SVGPathParserFactory::buildSVGPathSegListFromString(const String& d, SVGPathSegList* result, PathParsingMode parsingMode)
{
    ASSERT(result);
    if (d.isEmpty())
        return false;

    SVGPathSegListBuilder* builder = globalSVGPathSegListBuilder();
    builder->setCurrentSVGPathSegList(result);

    SVGPathParser* parser = globalSVGPathParser();
    parser->setCurrentConsumer(builder);

    OwnPtr<SVGPathStringSource> source = SVGPathStringSource::create(d);
    parser->setCurrentSource(source.get());

    bool ok = parser->parsePathDataFromSource(parsingMode);
    parser->setCurrentConsumer(0);
    parser->setCurrentSource(0);
    builder->setCurrentSVGPathSegList(0);
    return ok;
}

bool SVGPathParserFactory::buildSVGPathSegListFromByteStream(SVGPathByteStream* stream, SVGPathSegList* result, PathParsingMode parsingMode)
{
    ASSERT(stream);
    ASSERT(result);
    if (stream->isEmpty())
        return false; 

    SVGPathSegListBuilder* builder = globalSVGPathSegListBuilder();
    builder->setCurrentSVGPathSegList(result);

    SVGPathParser* parser = globalSVGPathParser();
    parser->setCurrentConsumer(builder);

    OwnPtr<SVGPathByteStreamSource> source = SVGPathByteStreamSource::create(stream);
    parser->setCurrentSource(source.get());

    bool ok = parser->parsePathDataFromSource(parsingMode);
    parser->setCurrentConsumer(0);
    parser->setCurrentSource(0);
    builder->setCurrentSVGPathSegList(0);
    return ok;
}

PassOwnPtr<SVGPathByteStream> SVGPathParserFactory::createSVGPathByteStreamFromString(const String& d, PathParsingMode parsingMode, bool& ok)
{
    if (d.isEmpty()) {
        ok = false;
        return PassOwnPtr<SVGPathByteStream>();
    }

    SVGPathByteStreamBuilder* builder = globalSVGPathByteStreamBuilder();
    OwnPtr<SVGPathByteStream> stream = SVGPathByteStream::create();
    builder->setCurrentByteStream(stream.get());

    SVGPathParser* parser = globalSVGPathParser();
    parser->setCurrentConsumer(builder);

    OwnPtr<SVGPathStringSource> source = SVGPathStringSource::create(d);
    parser->setCurrentSource(source.get());

    ok = parser->parsePathDataFromSource(parsingMode);
    parser->setCurrentConsumer(0);
    parser->setCurrentSource(0);
    builder->setCurrentByteStream(0);
    return stream.release();
}

}

#endif
