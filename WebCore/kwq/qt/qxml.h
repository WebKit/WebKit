/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef QXML_H_
#define QXML_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class QString;

// class QXmlAttributes ========================================================

class QXmlAttributes {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
        
    QXmlAttributes();
    QXmlAttributes(const QXmlAttributes &);
    virtual ~QXmlAttributes();
    
    // member functions --------------------------------------------------------

    QString value(const QString &) const;
    int length() const;
    QString localName(int index) const;
    QString value(int index) const;

    // operators ---------------------------------------------------------------

    QXmlAttributes &operator=(const QXmlAttributes &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QXmlAttributes =====================================================


// class QXmlInputSource ========================================================

class QXmlInputSource {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QXmlInputSource();
    virtual ~QXmlInputSource();

    // member functions --------------------------------------------------------

    virtual void setData(const QString& data);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlInputSource(const QXmlInputSource &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlInputSource &operator=(const QXmlInputSource &);
#endif

}; // class QXmlInputSource ====================================================


// class QXmlDTDHandler ========================================================

class QXmlDTDHandler {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QXmlDTDHandler() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlDTDHandler() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlDTDHandler(const QXmlDTDHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlDTDHandler &operator=(const QXmlDTDHandler &);
#endif

}; // class QXmlDTDHandler =====================================================


// class QXmlDeclHandler ========================================================

class QXmlDeclHandler {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QXmlDeclHandler() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlDeclHandler() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlDeclHandler(const QXmlDeclHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlDeclHandler &operator=(const QXmlDeclHandler &);
#endif

}; // class QXmlDeclHandler =====================================================


// class QXmlErrorHandler ========================================================

class QXmlErrorHandler {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QXmlErrorHandler() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlErrorHandler() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlErrorHandler(const QXmlErrorHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlErrorHandler &operator=(const QXmlErrorHandler &);
#endif

}; // class QXmlErrorHandler =====================================================


// class QXmlLexicalHandler ========================================================

class QXmlLexicalHandler {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QXmlLexicalHandler() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlLexicalHandler() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlLexicalHandler(const QXmlLexicalHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlLexicalHandler &operator=(const QXmlLexicalHandler &);
#endif

}; // class QXmlLexicalHandler =====================================================


// class QXmlContentHandler ========================================================

class QXmlContentHandler {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QXmlContentHandler() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlContentHandler() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlContentHandler(const QXmlContentHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlContentHandler &operator=(const QXmlContentHandler &);
#endif

}; // class QXmlContentHandler =====================================================


// class QXmlDefaultHandler ====================================================

class QXmlDefaultHandler :
    public QXmlContentHandler, 
    public QXmlLexicalHandler, 
    public QXmlErrorHandler, 
    public QXmlDeclHandler, 
    public QXmlDTDHandler {

public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QXmlDefaultHandler() {}
    virtual ~QXmlDefaultHandler();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlDefaultHandler(const QXmlDefaultHandler &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlDefaultHandler &operator=(const QXmlDefaultHandler &);
#endif

}; // class QXmlDefaultHandler =====================================================


// class QXmlSimpleReader ======================================================

class QXmlSimpleReader {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QXmlSimpleReader();
    virtual ~QXmlSimpleReader();    

    // member functions --------------------------------------------------------

    void setContentHandler(QXmlContentHandler *handler);
    bool parse(const QXmlInputSource &input);
    void setLexicalHandler(QXmlLexicalHandler *handler);
    void setDTDHandler(QXmlDTDHandler *handler);
    void setDeclHandler(QXmlDeclHandler *handler);
    void setErrorHandler(QXmlErrorHandler *handler);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlSimpleReader(const QXmlSimpleReader &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlSimpleReader &operator=(const QXmlSimpleReader &);
#endif

}; // class QXmlSimpleReader ===================================================


// class QXmlParseException ====================================================

class QXmlParseException {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
        
    // constructors, copy constructors, and destructors ------------------------
    
    QXmlParseException();
    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QXmlParseException() {}
#endif
    
    // member functions --------------------------------------------------------

    QString message() const;
    int columnNumber() const;
    int lineNumber() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QXmlParseException(const QXmlParseException &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QXmlParseException &operator=(const QXmlParseException &);
#endif

}; // class QXmlParseException =================================================

#endif
