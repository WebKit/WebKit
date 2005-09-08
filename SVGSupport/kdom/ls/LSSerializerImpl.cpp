/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <qfile.h>
#include <qtextstream.h>

#include <kurl.h>

#include "kdom.h"
#include "kdomls.h"
#include <kdom/Helper.h>
#include "NodeImpl.h"
#include "Namespace.h"
#include "DOMString.h"
#include "EntityImpl.h"
#include "NotationImpl.h"
#include "DocumentImpl.h"
#include "LSOutputImpl.h"
#include "DOMStringImpl.h"
#include "kdomtraversal.h"
#include "LSExceptionImpl.h"
#include "LSSerializerImpl.h"
#include "NamedNodeMapImpl.h"
#include "NamedAttrMapImpl.h"
#include "DocumentTypeImpl.h"
#include "DOMConfigurationImpl.h"
#include "ProcessingInstructionImpl.h"

using namespace KDOM;

LSSerializerImpl::LSSerializerImpl() : Shared(), m_config(0), m_serializerFilter(0)
{
    m_newLine = new DOMStringImpl("\n");
    m_newLine->ref();
}

LSSerializerImpl::~LSSerializerImpl()
{
    if(m_newLine)
        m_newLine->deref();
    if(m_config)
        m_config->deref();
}

DOMConfigurationImpl *LSSerializerImpl::domConfig() const
{
    if(!m_config)
    {
        m_config = new DOMConfigurationImpl();
        m_config->ref();

        m_config->setParameter(FEATURE_NORMALIZE_CHARACTERS, true);
    }

    return m_config;
}

DOMStringImpl *LSSerializerImpl::newLine() const
{
    return m_newLine;
}

void LSSerializerImpl::setNewLine(DOMStringImpl *newLine)
{
    KDOM_SAFE_SET(m_newLine, newLine);
}

LSSerializerFilterImpl *LSSerializerImpl::filter() const
{
    return m_serializerFilter;
}

void LSSerializerImpl::setFilter(LSSerializerFilterImpl *filter)
{
    // TODO: implement me
    Q_UNUSED(filter);
}

bool LSSerializerImpl::serialize(NodeImpl *nodeArg, LSOutputImpl *output) const
{
#ifndef APPLE_COMPILE_HACK
    if(output && output->systemId() && !output->systemId()->isEmpty())
    {
        KURL url = KURL(output->systemId()->string());
        if(!url.isEmpty() && url.isLocalFile())
        {
            QFile f(url.path());
            if(f.open(IO_WriteOnly))
            {
                QTextStream out(&f);
                PrintNode(out, nodeArg, QString::fromLatin1("  "), DOMString(m_newLine).string(), 0, m_config);
                f.close();
            }
        }
        else
            return false;
    }
    else
    {
        // SERIALIZE_ERR: Raised if the LSSerializer was unable to serialize the node.
        // DOM applications should attach a DOMErrorHandler using the parameter
        // "error-handler" if they wish to get details on the error.
        throw new LSExceptionImpl(SERIALIZE_ERR);
    }
#endif
    return true;
}

bool LSSerializerImpl::write(NodeImpl *nodeArg, LSOutputImpl *output)
{
    return serialize(nodeArg, output);
}

bool LSSerializerImpl::writeToURI(NodeImpl *nodeArg, DOMStringImpl *uri)
{
    LSOutputImpl output;
    output.setSystemId(uri);
    return serialize(nodeArg, &output);
}

DOMStringImpl *LSSerializerImpl::writeToString(NodeImpl *nodeArg)
{
    QString str;
    QTextOStream subset(&str);
    PrintNode(subset, nodeArg, QString::fromLatin1("  "), DOMString(m_newLine).string(), 0, m_config);
    return new DOMStringImpl(str);
}

void LSSerializerImpl::PrintInternalSubset(QTextStream &ret, DocumentTypeImpl *docType,
                                          bool prettyIndent, unsigned short level,
                                          const QString& indentStr)
{
    NamedNodeMapImpl *entities = docType->entities();
    NamedNodeMapImpl *notations = docType->notations();
    unsigned long nrEntities = entities->length();
    unsigned long nrNotations = notations->length();
    for(unsigned long i = 0; i < nrEntities; i++)
    {
        if(prettyIndent)
            PrintIndentation(ret, level, indentStr);

        EntityImpl *ent = static_cast<EntityImpl *>(entities->item(i));

        ret << QString::fromLatin1("<!ENTITY %1").arg(DOMString(ent->nodeName()).string());
        if(ent->publicId() && !ent->publicId()->isEmpty())
        {
            ret << QString::fromLatin1(" PUBLIC \"%1\"").arg(DOMString(ent->publicId()).string());
            if(ent->systemId() && !ent->systemId()->isEmpty())
                ret << QString::fromLatin1(" \"%1\"").arg(DOMString(ent->systemId()).string());
            if(ent->notationName() && !ent->notationName()->isEmpty())
                ret << QString::fromLatin1(" NDATA \"%1\"").arg(DOMString(ent->notationName()).string());
            ret << QString::fromLatin1(">\n");
        }
        else if(ent->systemId() && !ent->systemId()->isEmpty())
        {
            ret << QString::fromLatin1(" SYSTEM \"%1\"").arg(DOMString(ent->systemId()).string());
            if(ent->notationName() && !ent->notationName()->isEmpty())
                ret << QString::fromLatin1(" NDATA \"%1\"").arg(DOMString(ent->notationName()).string());
            ret << QString::fromLatin1(">\n");
        }
        else
        {
            NodeImpl *entImpl = static_cast<NodeImpl *>(ent);
            ret << QString::fromLatin1(" \"%1\">\n").arg(DOMString(entImpl->toString()).string().remove('\n'));
        }
    }

    for(unsigned long i = 0; i < nrNotations; i++)
    {
        if(prettyIndent)
            PrintIndentation(ret, level, indentStr);

        NotationImpl *notation = static_cast<NotationImpl *>(notations->item(i));

        if(!notation->publicId() || notation->publicId()->isEmpty())
        {
            ret << QString::fromLatin1("<!NOTATION ") <<
                   DOMString(notation->nodeName()).string() +
                   QString::fromLatin1(" SYSTEM \"%1\">\n").arg(DOMString(notation->systemId()).string());
        }
        else
        {
            ret << QString::fromLatin1("<!NOTATION ") <<
                   DOMString(notation->nodeName()).string() +
                   QString::fromLatin1(" PUBLIC \"%1\">\n").arg(DOMString(notation->publicId()).string());
        }
    }
}

void LSSerializerImpl::PrintNode(QTextStream &ret, NodeImpl *node, const QString &indentStr,
                                 const QString &newLine,
                                 unsigned short level, DOMConfigurationImpl *config)
{
    if(!node) return;
    bool prettyIndent = (!config || config->getParameter(FEATURE_FORMAT_PRETTY_PRINT));

    switch(node->nodeType())
    {
        case DOCUMENT_NODE:
        {
            DocumentImpl *document = static_cast<DocumentImpl *>(node);

            if(!config || config->getParameter(FEATURE_XML_DECLARATION))
            {
                ret << "<?xml version=\"" << DOMString(document->xmlVersion()).string() << "\"";

                DOMString encoding(document->xmlEncoding());
                if(!encoding.isEmpty())
                       ret << " encoding=\"" << encoding.string() << "\"";

                if(document->xmlStandalone())
                       ret << " standalone='yes'";

                ret << "?>" << newLine;
            }

            for(NodeImpl *child = node->firstChild(); child != 0; child = child->nextSibling())
                PrintNode(ret, child, indentStr, newLine, 0, config);

            break;
        }
        case DOCUMENT_TYPE_NODE:
        {
            PrintIndentation(ret, level, indentStr);

            DocumentTypeImpl *docType = static_cast<DocumentTypeImpl *>((node));
            if(!docType->publicId() || docType->publicId()->isEmpty())
            {
                if(!docType->systemId() || docType->systemId()->isEmpty())
                {
                    ret << QString::fromLatin1("<!DOCTYPE ") << DOMString(docType->nodeName()).string();
                }
                else
                {
                    ret << QString::fromLatin1("<!DOCTYPE ") << DOMString(docType->nodeName()).string() <<
                               QString::fromLatin1(" SYSTEM \"") << DOMString(docType->systemId()).string() <<
                            QString::fromLatin1("\"") << newLine;
                }
            }
            else
            {
                ret << QString::fromLatin1("<!DOCTYPE ") << DOMString(docType->nodeName()).string() <<
                       QString::fromLatin1(" PUBLIC \"") << DOMString(docType->publicId()).string() <<
                       QString::fromLatin1("\" \"") << DOMString(docType->systemId()).string() <<
                       QString::fromLatin1("\"") << newLine;
            }

            // handle entities + notations
            NamedNodeMapImpl *entities = docType->entities();
            unsigned long nrEntities = entities->length();

            NamedNodeMapImpl *notations = docType->notations();
            unsigned long nrNotations = notations->length();

            if(nrEntities + nrNotations > 0)
            {
                ret << "[" << newLine;
                PrintInternalSubset(ret, docType, prettyIndent, level + 1, indentStr);
                ret << "]" << newLine;
            }

            ret << ">" << newLine;
            break;
        }
        case TEXT_NODE:
        {
            if(acceptNode() == FILTER_ACCEPT)
                ret << DOMString(escape(node->nodeValue())).string();
            break;
        }
        case CDATA_SECTION_NODE:
        {
            if((!config || config->getParameter(FEATURE_CDATA_SECTIONS)) && acceptNode() == FILTER_ACCEPT)
            {
                ret << QString::fromLatin1("<![CDATA[") + DOMString(node->nodeValue()).string();
                ret << QString::fromLatin1("]]>");
            }
            else
                ret << DOMString(node->nodeValue()).string();

            if(prettyIndent)
                ret << newLine;

            break;
        }
        case COMMENT_NODE:
        {
            if((!config || config->getParameter(FEATURE_COMMENTS)) && acceptNode())
            {
                if(prettyIndent)
                    PrintIndentation(ret, level, indentStr);

                ret << QString::fromLatin1("<!--") + DOMString(node->nodeValue()).string() + QString::fromLatin1("-->");

                if(prettyIndent)
                    ret << newLine;
            }
            break;
        }
        case PROCESSING_INSTRUCTION_NODE:
        {
            if(prettyIndent)
                PrintIndentation(ret, level, indentStr);

            ProcessingInstructionImpl *pi = static_cast<ProcessingInstructionImpl *>(node);
            ret << QString::fromLatin1("<?") + DOMString(pi->target()).string() +
                   QString::fromLatin1(" ") + DOMString(pi->data()).string() +
                   QString::fromLatin1("?>");

            if(prettyIndent)
                ret << newLine;

            break;
        }
        case ENTITY_NODE:
        {
            // just process children
            if(node->firstChild() != 0)
                for(NodeImpl *child = node->firstChild(); child != 0; child = child->nextSibling())
                    PrintNode(ret, child, indentStr, newLine, 0, config);

            break;
        }
        case ENTITY_REFERENCE_NODE:
        {
            // just process children
            if(node->firstChild() != 0)
                for(NodeImpl *child = node->firstChild(); child != 0; child = child->nextSibling())
                    PrintNode(ret, child, indentStr, newLine, 0, config);

            break;
        }
        case ATTRIBUTE_NODE:
        {
            bool isNsDecl = (DOMString(node->namespaceURI()) == NS_XMLNS && (config && config->getParameter(FEATURE_NAMESPACES)));
            if(!isNsDecl && (!config || config->getParameter(FEATURE_NAMESPACE_DECLARATIONS)))
            {
                ret << DOMString(node->nodeName()).string() <<
                       QString::fromLatin1("=\"") <<
                       DOMString(escapeAttribute(node->nodeValue())).string() <<
                       QString::fromLatin1("\"");
            }

            break;
        }
        default:
        {
            if(prettyIndent)
                PrintIndentation(ret, level, indentStr);

            ret << QString::fromLatin1("<") + DOMString(node->nodeName()).string();

            // Print attributes
            NamedNodeMapImpl *map = node->attributes();
            unsigned int nrAttrs = map->length();

            for(unsigned int i = 0; i < nrAttrs; ++i)
            {
                NodeImpl *n = map->item(i);
                bool isNsDecl = (DOMString(node->namespaceURI()) == NS_XMLNS && (config && config->getParameter(FEATURE_NAMESPACES)));
                if(n && !isNsDecl && (!config || config->getParameter(FEATURE_NAMESPACE_DECLARATIONS)))
                {
                    ret << QString::fromLatin1(" ") <<
                           DOMString(n->nodeName()).string() <<
                           QString::fromLatin1("=\"") <<
                           DOMString(n->nodeValue()).string() <<
                           QString::fromLatin1("\"");
                }
            }

            if(node->firstChild() == 0) // No children
            {
                ret << "/>";

                if(prettyIndent)
                    ret << newLine;
            }
            else // Handle children
            {
                ret << ">";
                if(prettyIndent && node->firstChild()->nodeType() != TEXT_NODE &&
                   node->firstChild()->nodeType() != CDATA_SECTION_NODE)
                {
                    ret << newLine;
                }

                bool indent = false;

                for(NodeImpl *child = node->firstChild(); child != 0; child = child->nextSibling())
                {
                    PrintNode(ret, child, indentStr, newLine, level + 1, config);
                    if(child->nodeType() != TEXT_NODE)
                        indent = true;
                }

                if(prettyIndent && indent)
                    PrintIndentation(ret, level, indentStr);

                ret << QString::fromLatin1("</") <<
                       DOMString(node->nodeName()).string() <<
                       QString::fromLatin1(">");

                if(prettyIndent)
                    ret << newLine;
            }
        }
    }
}

void LSSerializerImpl::PrintIndentation(QTextStream &ret, unsigned short level, const QString &indent)
{
    for(unsigned short i = 0; i < level; i++)
        ret << indent;
}

DOMStringImpl *LSSerializerImpl::escape(DOMStringImpl *escapeeImpl)
{
    DOMString escapee(escapeeImpl);

    DOMStringImpl *result = new DOMStringImpl();
    const unsigned int length = escapee.length();

    for(unsigned int i = 0; i < length; ++i)
    {
        if(escapee[i] == '<')
            result->append("&lt;");
        else if(escapee[i] == '>')
            result->append("&gt;");
        else if(escapee[i] == '&')
            result->append("&amp;");
        else
            result->append(QString(escapee[i]));
    }

    return result;
}

DOMStringImpl *LSSerializerImpl::escapeAttribute(DOMStringImpl *escapeeImpl)
{
    DOMString escapee(escapeeImpl);

    /* Loop copied from escape() in order to save one iteration. */
    DOMStringImpl *result = new DOMStringImpl();
    const unsigned int length = escapee.length();

    for(unsigned int i = 0; i < length; ++i)
    {
        if(escapee[i] == '<')
            result->append("&lt;");
        else if(escapee[i] == '>')
            result->append("&gt;");
        else if(escapee[i] == '&')
            result->append("&amp;");
        else if(escapee[i] == '\"')
            result->append("&quot;");
        else if(escapee[i] == '\'')
            result->append("&apos;");
        else
            result->append(QString(escapee[i]));
    }

    return result;
}

long LSSerializerImpl::acceptNode()
{
    // TODO : check filter here
    return FILTER_ACCEPT;
}

// vim:ts=4:noet
