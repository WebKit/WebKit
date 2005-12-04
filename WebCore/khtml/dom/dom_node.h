/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 1 Specification (Recommendation)
 * http://www.w3.org/TR/REC-DOM-Level-1/
 * Copyright � World Wide Web Consortium , (Massachusetts Institute of
 * Technology , Institut National de Recherche en Informatique et en
 * Automatique , Keio University ). All Rights Reserved.
 *
 */
#ifndef _DOM_Node_h_
#define _DOM_Node_h_

namespace DOM {

/**
 * The <code> Node </code> interface is the primary datatype for the
 * entire Document Object Model. It represents a single node in the
 * document tree. While all objects implementing the <code> Node
 * </code> interface expose methods for dealing with children, not all
 * objects implementing the <code> Node </code> interface may have
 * children. For example, <code> Text </code> nodes may not have
 * children, and adding children to such nodes results in a <code>
 * DOMException </code> being raised.
 *
 *  The attributes <code> nodeName </code> , <code> nodeValue </code>
 * and <code> attributes </code> are included as a mechanism to get at
 * node information without casting down to the specific derived
 * interface. In cases where there is no obvious mapping of these
 * attributes for a specific <code> nodeType </code> (e.g., <code>
 * nodeValue </code> for an Element or <code> attributes </code> for a
 * Comment), this returns <code> null </code> . Note that the
 * specialized interfaces may contain additional and more convenient
 * mechanisms to get and set the relevant information.
 *
 */
class Node
{

public:

    /**
     * An integer indicating which type of node this is.
     *
     *
     * <p>The values of <code>nodeName</code>, <code>nodeValue</code>,
     *  and <code>attributes</code> vary according to the node type as follows:
     *   <table  border="1">
     *     <tbody>
     *     <tr>
     *       <td></td>
     *       <td>nodeName</td>
     *       <td>nodeValue</td>
     *       <td>attributes</td>
     *     </tr>
     *     <tr>
     *       <td>Element</td>
     *       <td>tagName</td>
     *       <td>null</td>
     *       <td>NamedNodeMap</td>
     *     </tr>
     *     <tr>
     *       <td>Attr</td>
     *       <td>name of attribute</td>
     *       <td>value of attribute</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>Text</td>
     *       <td>#text</td>
     *       <td>content of the text node</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>CDATASection</td>
     *       <td>#cdata-section</td>
     *       <td>content of the CDATA Section</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>EntityReference</td>
     *       <td>name of entity referenced</td>
     *       <td>null</td>
     *       <td>null</td>
     *       </tr>
     *     <tr>
     *       <td>Entity</td>
     *       <td>entity name</td>
     *       <td>null</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>ProcessingInstruction</td>
     *       <td>target</td>
     *       <td>entire content excluding the target</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>Comment</td>
     *       <td>#comment</td>
     *       <td>content of the comment</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>Document</td>
     *       <td>#document</td>
     *       <td>null</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>DocumentType</td>
     *       <td>document type name</td>
     *       <td>null</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>DocumentFragment</td>
     *       <td>#document-fragment</td>
     *       <td>null</td>
     *       <td>null</td>
     *     </tr>
     *     <tr>
     *       <td>Notation</td>
     *       <td>notation name</td>
     *       <td>null</td>
     *       <td>null</td>
     *     </tr>
     *     </tbody>
     *   </table>
     * </p>
     */
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
        NOTATION_NODE = 12
    };
};

/**
 * A DOMTimeStamp represents a number of milliseconds.
 *
 */
#if __APPLE__
typedef unsigned long long DOMTimeStamp;
#else
typedef unsigned long DOMTimeStamp;
#endif

} //namespace

#endif
