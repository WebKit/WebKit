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

#ifndef KDOM_DOMImplementationLS_H
#define KDOM_DOMImplementationLS_H

#include <kdom/kdom.h>

namespace KDOM
{
	class LSParser;
	class LSSerializer;
	class LSInput;
	class LSOutput;

	/**
	 * @short DOMImplementationLS contains the factory methods for creating Load and
	 * Save objects. 
	 *
	 * The expectation is that an instance of the DOMImplementationLS interface can be
	 * obtained by using binding-specific casting methods on an instance of the
	 * DOMImplementation interface or, if the Document supports the feature "Core"
	 * version "3.0" defined in [DOM Level 3 Core], by using the method
	 * DOMImplementation.getFeature with parameter values "LS" (or "LS-Async") and
	 * "3.0" (respectively).
	 *
	 * @author Rob Buis <rwlbuis@kde.org>
	 */
	class DOMImplementationLS
	{
	public:
		DOMImplementationLS();
		virtual ~DOMImplementationLS();

		/**
		 * Create a new LSParser. The newly constructed parser may then be configured
		 * by means of its DOMConfiguration object, and used to parse documents by
		 * means of its parse method.
		 *
		 * @param mode The mode argument is either MODE_SYNCHRONOUS or MODE_ASYNCHRONOUS,
		 * if mode is MODE_SYNCHRONOUS then the LSParser that is created will operate
		 * in synchronous mode, if it's MODE_ASYNCHRONOUS then the LSParser that is
		 * created will operate in asynchronous mode.
		 * @param schemaType An absolute URI representing the type of the schema language used
		 * during the load of a Document using the newly created LSParser. Note that no
		 * lexical checking is done on the absolute URI. In order to create a LSParser
		 * for any kind of schema types (i.e. the LSParser will be free to use any
		 * schema found), use the value null.
		 *
		 * @returns The newly created LSParser object. This LSParser is either
		 * synchronous or asynchronous depending on the value of the mode argument.
		 */
		virtual LSParser createLSParser(unsigned short mode, const DOMString &schemaType) const = 0;

		/**
		 * Create a new LSSerializer object.
		 *
		 * @returns The newly created LSSerializer object.
		 */
		virtual LSSerializer createLSSerializer() const = 0;

		/**
		 * Create a new empty input source object where LSInput.characterStream,
		 * LSInput.byteStream, LSInput.stringData LSInput.systemId,
		 * LSInput.publicId, LSInput.baseURI, and LSInput.encoding are
		 * null, and LSInput.certifiedText is false.
		 *
		 * @returns The newly created input object.
		 */
		virtual LSInput createLSInput() const = 0;

		/**
		 * Create a new empty output destination object where LSOutput.characterStream,
		 * LSOutput.byteStream, LSOutput.systemId, LSOutput.encoding are null.
		 *
		 * @returns The newly created output object.
		 */
		virtual LSOutput createLSOutput() const = 0;
	};
};

#endif

// vim:ts=4:noet
