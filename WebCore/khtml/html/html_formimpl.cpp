/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 */

#undef FORMS_DEBUG
//#define FORMS_DEBUG

#include "html/html_formimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "html/html_documentimpl.h"
#include "khtml_settings.h"
#include "misc/htmlhashes.h"

#include "css/cssstyleselector.h"
#include "css/cssproperties.h"
#include "css/csshelper.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "khtml_ext.h"

#include "rendering/render_form.h"

#include <kcharsets.h>
#include <kglobal.h>
#include <kdebug.h>
#include <kmimetype.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <netaccess.h>
#include <kfileitem.h>
#include <qfile.h>
#include <qtextcodec.h>

// for keygen
#include <qstring.h>
#include <ksslkeygen.h>

#include <assert.h>

using namespace DOM;
using namespace khtml;

HTMLFormElementImpl::HTMLFormElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_post = false;
    m_multipart = false;
    m_autocomplete = true;
    m_insubmit = false;
    m_doingsubmit = false;
    m_inreset = false;
    m_enctype = "application/x-www-form-urlencoded";
    m_boundary = "----------0xKhTmLbOuNdArY";
    m_acceptcharset = "UNKNOWN";
}

HTMLFormElementImpl::~HTMLFormElementImpl()
{
    QPtrListIterator<HTMLGenericFormElementImpl> it(formElements);
    for (; it.current(); ++it)
        it.current()->m_form = 0;
}

NodeImpl::Id HTMLFormElementImpl::id() const
{
    return ID_FORM;
}

long HTMLFormElementImpl::length() const
{
    int len = 0;
    QPtrListIterator<HTMLGenericFormElementImpl> it(formElements);
    for (; it.current(); ++it)
	if (it.current()->isEnumeratable())
	    ++len;

    return len;
}

static QCString encodeCString(const QCString& e)
{
    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    // safe characters like NS handles them for compatibility
    static const char *safe = "-._*";
    QCString encoded(( e.length()+e.contains( '\n' ) )*3+1);
    int enclen = 0;

    //QCString orig(e.data(), e.size());

    for(unsigned pos = 0; pos < e.length(); pos++) {
        unsigned char c = e[pos];

        if ( (( c >= 'A') && ( c <= 'Z')) ||
             (( c >= 'a') && ( c <= 'z')) ||
             (( c >= '0') && ( c <= '9')) ||
             (strchr(safe, c))
            )
            encoded[enclen++] = c;
        else if ( c == ' ' )
            encoded[enclen++] = '+';
        else if ( c == '\n' )
        {
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'D';
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'A';
        }
        else if ( c != '\r' )
        {
            encoded[enclen++] = '%';
            unsigned int h = c / 16;
            h += (h > 9) ? ('A' - 10) : '0';
            encoded[enclen++] = h;

            unsigned int l = c % 16;
            l += (l > 9) ? ('A' - 10) : '0';
            encoded[enclen++] = l;
        }
    }
    encoded[enclen++] = '\0';
    encoded.truncate(enclen);

    return encoded;
}

inline static QCString fixUpfromUnicode(const QTextCodec* codec, const QString& s)
{
    QCString str = codec->fromUnicode(s);
    str.truncate(str.length());
    return str;
}

void HTMLFormElementImpl::i18nData()
{
    QString foo1 = i18n( "You're about to send data to the Internet "
                         "via an unencrypted connection. It might be possible "
                         "for others to see this information.\n"
                         "Do you want to continue?");
    QString foo2 = i18n("KDE Web browser");
    QString foo3 = i18n("When you send a password unencrypted to the Internet, "
                        "it might be possible for others to capture it as plain text.\n"
                        "Do you want to continue?");
    QString foo5 = i18n("Your data submission is redirected to "
                        "an insecure site. The data is sent unencrypted.\n"
                        "Do you want to continue?");
    QString foo6 = i18n("The page contents expired. You can repost the form"
                        "data by using <a href=\"javascript:go(0);\">Reload</a>");
}


QByteArray HTMLFormElementImpl::formData(bool& ok)
{
#ifdef FORMS_DEBUG
    kdDebug( 6030 ) << "form: formData()" << endl;
#endif

    QByteArray form_data(0);
    QCString enc_string = ""; // used for non-multipart data

    // find out the QTextcodec to use
#ifdef APPLE_CHANGES
    QString origStr = m_acceptcharset.string();
    QChar space(' ');
    QChar strChars[origStr.length()];

    for(unsigned int i=0; i < origStr.length(); i++)
        if(origStr[i].latin1() == ',')
            strChars[i] = space;
        else
            strChars[i] = origStr[i];
    QString str(strChars, origStr.length());
#else /* APPLE_CHANGES not defined */
    QString str = m_acceptcharset.string();
    QChar space(' ');
    for(unsigned int i=0; i < str.length(); i++) if(str[i].latin1() == ',') str[i] = space;
#endif /* APPLE_CHANGES not defined */
    QStringList charsets = QStringList::split(' ', str);
    QTextCodec* codec = 0;
    KHTMLView *view = getDocument()->view();
    for ( QStringList::Iterator it = charsets.begin(); it != charsets.end(); ++it )
    {
        QString enc = (*it);
        if(enc.contains("UNKNOWN"))
        {
            // use standard document encoding
            enc = "ISO 8859-1";
            if(view && view->part())
                enc = view->part()->encoding();
        }
        if((codec = KGlobal::charsets()->codecForName(enc.latin1())))
            break;
    }

    if(!codec)
        codec = QTextCodec::codecForLocale();

    // we need to map visual hebrew to logical hebrew, as the web
    // server alsways expects responses in logical ordering
    if ( codec->mibEnum() == 11 )
	codec = QTextCodec::codecForMib( 85 );

#ifdef APPLE_CHANGES
    QString encCharset = codec->name();
    QChar encChars[encCharset.length()];
    for(unsigned int i=0; i < encCharset.length(); i++)
        encChars[i] = encCharset[i].latin1() == ' ' ? QChar('-') : encCharset[i].lower();
    QString m_encCharset(encChars,  encCharset.length());
#else /* APPLE_CHANGES not defined */
    m_encCharset = codec->name();
    for(unsigned int i=0; i < m_encCharset.length(); i++)
        m_encCharset[i] = m_encCharset[i].latin1() == ' ' ? QChar('-') : m_encCharset[i].lower();
#endif /* APPLE_CHANGES not defined */

    QStringList fileUploads;

    for (QPtrListIterator<HTMLGenericFormElementImpl> it(formElements); it.current(); ++it) {
        HTMLGenericFormElementImpl* current = it.current();
        khtml::encodingList lst;

        if (!current->disabled() && current->encoding(codec, lst, m_multipart))
        {
            //kdDebug(6030) << "adding name " << current->name().string() << endl;
            khtml::encodingList::Iterator it;
            for( it = lst.begin(); it != lst.end(); ++it )
            {
                if (!m_multipart)
                {
                    // handle ISINDEX / <input name=isindex> special
                    // but only if its the first entry
                    if ( enc_string.isEmpty() && *it == "isindex" ) {
                        ++it;
                        enc_string += encodeCString( *it );
                    }
                    else {
                        if(!enc_string.isEmpty())
                            enc_string += '&';

                        enc_string += encodeCString(*it);
                        enc_string += "=";
                        ++it;
                        enc_string += encodeCString(*it);
                    }
                }
                else
                {
                    QCString hstr("--");
                    hstr += m_boundary.string().latin1();
                    hstr += "\r\n";
                    hstr += "Content-Disposition: form-data; name=\"";
                    hstr += (*it).data();
                    hstr += "\"";

                    // if the current type is FILE, then we also need to
                    // include the filename
                    if (current->nodeType() == Node::ELEMENT_NODE && current->id() == ID_INPUT &&
                        static_cast<HTMLInputElementImpl*>(current)->inputType() == HTMLInputElementImpl::FILE)
                    {
                        QString path = static_cast<HTMLInputElementImpl*>(current)->value().string();
                        if (path.length()) fileUploads << path;
                        QString onlyfilename = path.mid(path.findRev('/')+1);

                        hstr += ("; filename=\"" + onlyfilename + "\"").ascii();
                        if(!static_cast<HTMLInputElementImpl*>(current)->value().isEmpty())
                        {
                            hstr += "\r\nContent-Type: ";
                            KMimeType::Ptr ptr = KMimeType::findByURL(KURL(path));
                            hstr += ptr->name().ascii();
                        }
                    }

                    hstr += "\r\n\r\n";
                    ++it;

                    // append body
                    unsigned int old_size = form_data.size();
                    form_data.resize( old_size + hstr.length() + (*it).size() + 1);
                    memcpy(form_data.data() + old_size, hstr.data(), hstr.length());
                    memcpy(form_data.data() + old_size + hstr.length(), *it, (*it).size());
                    form_data[form_data.size()-2] = '\r';
                    form_data[form_data.size()-1] = '\n';
                }
            }
        }
    }

#ifndef APPLE_CHANGES
    if (fileUploads.count()) {
        int result = KMessageBox::warningContinueCancelList( 0,
                                                             i18n("You're about to transfer the following files from "
                                                                  "your local computer to the Internet.\n"
                                                                  "Do you really want to continue?"),
                                                             fileUploads);


        if (result == KMessageBox::Cancel) {
            ok = false;
            return QByteArray();
        }
    }
#endif

    if (m_multipart)
        enc_string = ("--" + m_boundary.string() + "--\r\n").ascii();

    int old_size = form_data.size();
    form_data.resize( form_data.size() + enc_string.length() );
    memcpy(form_data.data() + old_size, enc_string.data(), enc_string.length() );

    ok = true;
    return form_data;
}

void HTMLFormElementImpl::setEnctype( const DOMString& type )
{
    if(type.string().find("multipart", 0, false) != -1 || type.string().find("form-data", 0, false) != -1)
    {
        m_enctype = "multipart/form-data";
        m_multipart = true;
        m_post = true;
    } else if (type.string().find("text", 0, false) != -1 || type.string().find("plain", 0, false) != -1)
    {
        m_enctype = "text/plain";
        m_multipart = false;
    }
    else
    {
        m_enctype = "application/x-www-form-urlencoded";
        m_multipart = false;
    }
    m_encCharset = QString::null;
}

void HTMLFormElementImpl::setBoundary( const DOMString& bound )
{
    m_boundary = bound;
}

bool HTMLFormElementImpl::prepareSubmit()
{
    KHTMLView *view = getDocument()->view();
    if(m_insubmit || !view || !view->part() || view->part()->onlyLocalReferences())
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    if ( dispatchHTMLEvent(EventImpl::SUBMIT_EVENT,true,true) && !m_doingsubmit )
        m_doingsubmit = true;

    m_insubmit = false;

    if ( m_doingsubmit )
        submit();

    return m_doingsubmit;
}

void HTMLFormElementImpl::submit(  )
{
    if ( m_insubmit ) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

#ifdef FORMS_DEBUG
    kdDebug( 6030 ) << "submitting!" << endl;
#endif

    KHTMLView *view = getDocument()->view();
    for (QPtrListIterator<HTMLGenericFormElementImpl> it(formElements); it.current(); ++it) {
        HTMLGenericFormElementImpl* current = it.current();
        if (current->id() == ID_INPUT &&
            static_cast<HTMLInputElementImpl*>(current)->inputType() == HTMLInputElementImpl::TEXT &&
            static_cast<HTMLInputElementImpl*>(current)->autoComplete() )
        {
            HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl *>(current);
            view->addFormCompletionItem(input->name().string(), input->value().string());
        }
    }

    bool ok;
    QByteArray form_data = formData(ok);
    if (ok) {
        if(m_post) {
            view->part()->submitForm( "post", m_url.string(), form_data,
                                      m_target.string(),
                                      enctype().string(),
                                      boundary().string() );
        }
        else {
            view->part()->submitForm( "get", m_url.string(), form_data,
                                      m_target.string() );
        }
    }

    m_doingsubmit = m_insubmit = false;
}

void HTMLFormElementImpl::reset(  )
{
    KHTMLView *view = getDocument()->view();
    if(m_inreset || !view || !view->part()) return;

    m_inreset = true;

#ifdef FORMS_DEBUG
    kdDebug( 6030 ) << "reset pressed!" << endl;
#endif

    // ### DOM2 labels this event as not cancelable, however
    // common browsers( sick! ) allow it be cancelled.
    if ( !dispatchHTMLEvent(EventImpl::RESET_EVENT,true, true) ) {
        m_inreset = false;
        return;
    }

    for (QPtrListIterator<HTMLGenericFormElementImpl> it(formElements); it.current(); ++it)
        it.current()->reset();

    m_inreset = false;
}

void HTMLFormElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ACTION:
        m_url = khtml::parseURL(attr->value());
        break;
    case ATTR_TARGET:
        m_target = attr->value();
        break;
    case ATTR_METHOD:
        if ( strcasecmp( attr->value(), "post" ) == 0 )
            m_post = true;
        break;
    case ATTR_ENCTYPE:
        setEnctype( attr->value() );
        break;
    case ATTR_ACCEPT_CHARSET:
        // space separated list of charsets the server
        // accepts - see rfc2045
        m_acceptcharset = attr->value();
        break;
    case ATTR_ACCEPT:
        // ignore this one for the moment...
        break;
    case ATTR_AUTOCOMPLETE:
        m_autocomplete = strcasecmp( attr->value(), "off" );
        break;
    case ATTR_ONSUBMIT:
        setHTMLEventListener(EventImpl::SUBMIT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONRESET:
        setHTMLEventListener(EventImpl::RESET_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ID:
    case ATTR_NAME:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLFormElementImpl::radioClicked( HTMLGenericFormElementImpl *caller )
{
    for (QPtrListIterator<HTMLGenericFormElementImpl> it(formElements); it.current(); ++it) {
        HTMLGenericFormElementImpl *current = it.current();
        if (current->id() == ID_INPUT &&
            static_cast<HTMLInputElementImpl*>(current)->inputType() == HTMLInputElementImpl::RADIO &&
            current != caller && current->form() == caller->form() && current->name() == caller->name()) {
            static_cast<HTMLInputElementImpl*>(current)->setChecked(false);
        }
    }
}

void HTMLFormElementImpl::registerFormElement(HTMLGenericFormElementImpl *e)
{
    formElements.append(e);
}

void HTMLFormElementImpl::removeFormElement(HTMLGenericFormElementImpl *e)
{
    formElements.remove(e);
}

// -------------------------------------------------------------------------

HTMLGenericFormElementImpl::HTMLGenericFormElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLElementImpl(doc)
{
    m_disabled = m_readOnly = false;
    m_name = 0;

    if (f)
	m_form = f;
    else
	m_form = getForm();
    if (m_form)
        m_form->registerFormElement(this);
}

HTMLGenericFormElementImpl::~HTMLGenericFormElementImpl()
{
    if (m_form)
        m_form->removeFormElement(this);
}

void HTMLGenericFormElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_NAME:
        break;
    case ATTR_DISABLED:
        setDisabled( attr->val() != 0 );
        break;
    case ATTR_READONLY:
    {
        bool m_oldreadOnly = m_readOnly;
        m_readOnly = attr->val() != 0;
        if (m_oldreadOnly != m_readOnly) setChanged();
        break;
    }
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLGenericFormElementImpl::attach()
{
    assert(!attached());

    if (m_render) {
        assert(m_render->style());
        parentNode()->renderer()->addChild(m_render, nextRenderer());
    }

    NodeBaseImpl::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (m_render)
        m_render->updateFromElement();
}

HTMLFormElementImpl *HTMLGenericFormElementImpl::getForm() const
{
    NodeImpl *p = parentNode();
    while(p)
    {
        if( p->id() == ID_FORM )
            return static_cast<HTMLFormElementImpl *>(p);
        p = p->parentNode();
    }
#ifdef FORMS_DEBUG
    kdDebug( 6030 ) << "couldn't find form!" << endl;
#endif
    return 0;
}

DOMString HTMLGenericFormElementImpl::name() const
{
    if (m_name) return m_name;

// ###
//     DOMString n = getDocument()->htmlMode() != DocumentImpl::XHtml ?
//                   getAttribute(ATTR_NAME) : getAttribute(ATTR_ID);
    DOMString n = getAttribute(ATTR_NAME);
    if (n.isNull())
        return new DOMStringImpl("");

    return n;
}

void HTMLGenericFormElementImpl::setName(const DOMString& name)
{
    if (m_name) m_name->deref();
    m_name = name.implementation();
    if (m_name) m_name->ref();
}

void HTMLGenericFormElementImpl::onSelect()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(EventImpl::SELECT_EVENT,true,false);
}

void HTMLGenericFormElementImpl::onChange()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(EventImpl::CHANGE_EVENT,true,false);
}

void HTMLGenericFormElementImpl::setDisabled( bool _disabled )
{
    if ( m_disabled != _disabled ) {
        m_disabled = _disabled;
        setChanged();
    }
}

void HTMLGenericFormElementImpl::setParent(NodeImpl *parent)
{
    if (parentNode()) { // false on initial insert, we use the form given by the parser
	if (m_form)
	    m_form->removeFormElement(this);
	m_form = 0;
    }
    HTMLElementImpl::setParent(parent);
    if (!m_form) {
	m_form = getForm();
	if (m_form)
	    m_form->registerFormElement(this);
    }
}

void HTMLGenericFormElementImpl::recalcStyle( StyleChange ch )
{
    //bool changed = changed();
    HTMLElementImpl::recalcStyle( ch );

    if (m_render /*&& changed*/)
        m_render->updateFromElement();
}


bool HTMLGenericFormElementImpl::isSelectable() const
{
    return  m_render && m_render->isWidget() &&
        static_cast<RenderWidget*>(m_render)->widget() &&
        static_cast<RenderWidget*>(m_render)->widget()->focusPolicy() >= QWidget::TabFocus;
}

void HTMLGenericFormElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->target()==this)
    {
        // Report focus in/out changes to the browser extension (editable widgets only)
        KHTMLView *view = getDocument()->view();
        if (evt->id()==EventImpl::DOMFOCUSIN_EVENT && isEditable() && m_render && m_render->isWidget()) {
            KHTMLPartBrowserExtension *ext = static_cast<KHTMLPartBrowserExtension *>(view->part()->browserExtension());
            QWidget *widget = static_cast<RenderWidget*>(m_render)->widget();
            if (ext)
                ext->editableWidgetFocused(widget);
        }
        if (evt->id()==EventImpl::MOUSEDOWN_EVENT || evt->id()==EventImpl::KHTML_KEYDOWN_EVENT)
        {
            setActive();
        }
        else if (evt->id() == EventImpl::MOUSEUP_EVENT || evt->id()==EventImpl::KHTML_KEYUP_EVENT)
        {
	    if (m_active)
	    {
		setActive(false);
		setFocus();
	    }
	    else {
                setActive(false);
            }
        }

	if (evt->id()==EventImpl::KHTML_KEYDOWN_EVENT ||
	    evt->id()==EventImpl::KHTML_KEYUP_EVENT)
	{
	    KeyEventImpl * k = static_cast<KeyEventImpl *>(evt);
	    if (k->keyVal() == QChar('\n').unicode() && m_render && m_render->isWidget() && k->qKeyEvent)
		QApplication::sendEvent(static_cast<RenderWidget *>(m_render)->widget(), k->qKeyEvent);
	}

	if (evt->id()==EventImpl::DOMFOCUSOUT_EVENT && isEditable() && m_render->isWidget()) {
	    KHTMLPartBrowserExtension *ext = static_cast<KHTMLPartBrowserExtension *>(view->part()->browserExtension());
	    QWidget *widget = static_cast<RenderWidget*>(m_render)->widget();
	    if (ext)
		ext->editableWidgetBlurred(widget);

	    // ### Don't count popup as a valid reason for losing the focus (example: opening the options of a select
	    // combobox shouldn't emit onblur)
	}
    }
    HTMLElementImpl::defaultEventHandler(evt);
}

bool HTMLGenericFormElementImpl::isEditable()
{
    return false;
}

// -------------------------------------------------------------------------

HTMLButtonElementImpl::HTMLButtonElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
    m_clicked = false;
    m_type = SUBMIT;
    m_dirty = true;
    m_activeSubmit = false;
}

HTMLButtonElementImpl::~HTMLButtonElementImpl()
{
}

NodeImpl::Id HTMLButtonElementImpl::id() const
{
    return ID_BUTTON;
}

DOMString HTMLButtonElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLButtonElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_TYPE:
        if ( strcasecmp( attr->value(), "submit" ) == 0 )
            m_type = SUBMIT;
        else if ( strcasecmp( attr->value(), "reset" ) == 0 )
            m_type = RESET;
        else if ( strcasecmp( attr->value(), "button" ) == 0 )
            m_type = BUTTON;
        break;
    case ATTR_VALUE:
        m_value = attr->value();
        m_currValue = m_value.string();
        break;
    case ATTR_ACCESSKEY:
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::FOCUS_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        setHTMLEventListener(EventImpl::BLUR_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

void HTMLButtonElementImpl::attach()
{
    // skip the generic handler
    HTMLElementImpl::attach();
    // doesn't work yet in the renderer ### fixme
//     if (renderer())
//         renderer()->setReplaced(true);
}

void HTMLButtonElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (m_type != BUTTON && (evt->id() == EventImpl::DOMACTIVATE_EVENT)) {
        m_clicked = true;

        if(m_form && m_type == SUBMIT) {
            m_activeSubmit = true;
            m_form->prepareSubmit();
            m_activeSubmit = false; // in case we were canceled
        }
        if(m_form && m_type == RESET) m_form->reset();
    }
    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

bool HTMLButtonElementImpl::encoding(const QTextCodec* codec, khtml::encodingList& encoding, bool /*multipart*/)
{
    if (m_type != SUBMIT || name().isEmpty() || !m_activeSubmit)
        return false;

    encoding += fixUpfromUnicode(codec, name().string());
    QString enc_str = m_currValue.isNull() ? QString("") : m_currValue;
    encoding += fixUpfromUnicode(codec, enc_str);

    return true;
}


// -------------------------------------------------------------------------

HTMLFieldSetElementImpl::HTMLFieldSetElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
}

HTMLFieldSetElementImpl::~HTMLFieldSetElementImpl()
{
}

NodeImpl::Id HTMLFieldSetElementImpl::id() const
{
    return ID_FIELDSET;
}

void HTMLFieldSetElementImpl::attach()
{
    // Fieldsets need to at least get a render object so that the
    // children will be rendered. Eventually we need to create a
    // custom object that can draw the label within the grooved
    // border. -dwh
    return HTMLElementImpl::attach();
}

// -------------------------------------------------------------------------

HTMLInputElementImpl::HTMLInputElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
    m_type = TEXT;
    m_maxLen = -1;
    m_size = 20;
    m_clicked = false;
    m_checked = false;

    m_haveType = false;
    m_activeSubmit = false;
    m_autocomplete = true;

    xPos = 0;
    yPos = 0;

    if ( m_form )
        m_autocomplete = f->autoComplete();
}

HTMLInputElementImpl::~HTMLInputElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
}

NodeImpl::Id HTMLInputElementImpl::id() const
{
    return ID_INPUT;
}

void HTMLInputElementImpl::setType(const DOMString& /*t*/)
{
    // ###
}

DOMString HTMLInputElementImpl::type() const
{
    // needs to be lowercase according to DOM spec
    switch (m_type) {
    case TEXT: return "text";
    case PASSWORD: return "password";
    case CHECKBOX: return "checkbox";
    case RADIO: return "radio";
    case SUBMIT: return "submit";
    case RESET: return "reset";
    case FILE: return "file";
    case HIDDEN: return "hidden";
    case IMAGE: return "image";
    case BUTTON: return "button";
    default: return "";
    }
}

QString HTMLInputElementImpl::state( )
{
    switch (m_type) {
    case CHECKBOX:
    case RADIO:
        return QString::fromLatin1(m_checked ? "on" : "off");
    default:
        return value().string()+'.'; // Make sure the string is not empty!
    }
}

void HTMLInputElementImpl::restoreState(const QString &state)
{
    switch (m_type) {
    case CHECKBOX:
    case RADIO:
        setChecked((state == QString::fromLatin1("on")));
        break;
    default:
        setValue(DOMString(state.left(state.length()-1)));
        break;
    }
}

void HTMLInputElementImpl::select(  )
{
    if(!m_render) return;

    if (m_type == TEXT || m_type == PASSWORD)
        static_cast<RenderLineEdit*>(m_render)->select();
    else if (m_type == FILE)
        static_cast<RenderFileButton*>(m_render)->select();
}

void HTMLInputElementImpl::click(  )
{
    // ###
#ifdef FORMS_DEBUG
    kdDebug( 6030 ) << " HTMLInputElementImpl::click(  )" << endl;
#endif
}

void HTMLInputElementImpl::parseAttribute(AttributeImpl *attr)
{
    // ### IMPORTANT: check that the type can't be changed after the first time
    // otherwise a javascript programmer may be able to set a text field's value
    // to something like /etc/passwd and then change it to a file field
    switch(attr->id())
    {
    case ATTR_AUTOCOMPLETE:
        m_autocomplete = strcasecmp( attr->value(), "off" );
        break;
    case ATTR_TYPE: {
            typeEnum newType;

            if ( strcasecmp( attr->value(), "password" ) == 0 )
                newType = PASSWORD;
            else if ( strcasecmp( attr->value(), "checkbox" ) == 0 )
                newType = CHECKBOX;
            else if ( strcasecmp( attr->value(), "radio" ) == 0 )
                newType = RADIO;
            else if ( strcasecmp( attr->value(), "submit" ) == 0 )
                newType = SUBMIT;
            else if ( strcasecmp( attr->value(), "reset" ) == 0 )
                newType = RESET;
            else if ( strcasecmp( attr->value(), "file" ) == 0 )
                newType = FILE;
            else if ( strcasecmp( attr->value(), "hidden" ) == 0 )
                newType = HIDDEN;
            else if ( strcasecmp( attr->value(), "image" ) == 0 )
                newType = IMAGE;
            else if ( strcasecmp( attr->value(), "button" ) == 0 )
                newType = BUTTON;
            else if ( strcasecmp( attr->value(), "khtml_isindex" ) == 0 )
                newType = ISINDEX;
            else
                newType = TEXT;

            if (!m_haveType) {
                m_type = newType;
                m_haveType = true;
            }
            else if (m_type != newType) {
                setAttribute(ATTR_TYPE,type());
            }
        }
        break;
    case ATTR_VALUE:
    case ATTR_CHECKED:
        // these are the defaults, don't change them
        break;
    case ATTR_MAXLENGTH:
        m_maxLen = attr->val() ? attr->val()->toInt() : -1;
        setChanged();
        break;
    case ATTR_SIZE:
        m_size = attr->val() ? attr->val()->toInt() : 20;
        break;
    case ATTR_ALT:
    case ATTR_SRC:
        if (m_render && m_type == IMAGE) m_render->updateFromElement();
        break;
    case ATTR_USEMAP:
    case ATTR_ACCESSKEY:
        // ### ignore for the moment
        break;
    case ATTR_ALIGN:
        addHTMLAlignment( attr->value() );
        break;
    case ATTR_WIDTH:
        // ignore this attribute,  do _not_ add
        // a CSS_PROP_WIDTH here!
        // webdesigner are stupid - and IE/NS behave the same ( Dirk )
        break;
    case ATTR_HEIGHT:
        addCSSLength(CSS_PROP_HEIGHT, attr->value() );
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::FOCUS_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        setHTMLEventListener(EventImpl::BLUR_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONSELECT:
        setHTMLEventListener(EventImpl::SELECT_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCHANGE:
        setHTMLEventListener(EventImpl::CHANGE_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

void HTMLInputElementImpl::init()
{
    HTMLGenericFormElementImpl::init();

    // make sure we don't inherit a color to the form elements
    // by adding a non-CSS color property. this his higher
    // priority than inherited color, but lesser priority than
    // any color specified by CSS for the elements.
    switch( m_type ) {
    case TEXT:
    case PASSWORD:
    case ISINDEX:
    case FILE:
        addCSSProperty(CSS_PROP_COLOR, "text");
        break;
    case SUBMIT:
    case RESET:
    case BUTTON:
    case CHECKBOX:
    case RADIO:
        addCSSProperty(CSS_PROP_COLOR, "buttontext" );
    case HIDDEN:
    case IMAGE:
        break;
    };
    if (m_type != FILE) m_value = getAttribute(ATTR_VALUE);
    if ((uint) m_type <= ISINDEX && !m_value.isEmpty()) {
        QString value = m_value.string();
        // remove newline stuff..
        QString nvalue;
        for (unsigned int i = 0; i < value.length(); ++i)
            if (value[i] >= ' ')
                nvalue += value[i];
        m_value = nvalue;
    }
    m_checked = (getAttribute(ATTR_CHECKED) != 0);
}

void HTMLInputElementImpl::attach()
{
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

    RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
    _style->ref();
    if (parentNode()->renderer() && _style->display() != NONE) {
        switch(m_type)
        {
        case TEXT:
        case PASSWORD:
        case ISINDEX:      m_render = new RenderLineEdit(this);   break;
        case CHECKBOX:  m_render = new RenderCheckBox(this); break;
        case RADIO:        m_render = new RenderRadioButton(this); break;
        case SUBMIT:      m_render = new RenderSubmitButton(this); break;
        case IMAGE:       m_render =  new RenderImageButton(this); break;
        case RESET:      m_render = new RenderResetButton(this);   break;
        case FILE:         m_render =  new RenderFileButton(this);    break;
        case BUTTON:  m_render = new RenderPushButton(this);
        case HIDDEN:   break;
        }
    }

    if (m_render)
        m_render->setStyle(_style);

    HTMLGenericFormElementImpl::attach();
    _style->deref();
}

DOMString HTMLInputElementImpl::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElementImpl::altText()
    DOMString alt = getAttribute( ATTR_ALT );
    // fall back to title attribute
    if ( alt.isNull() )
        alt = getAttribute( ATTR_TITLE );
    if ( alt.isNull() )
        alt = getAttribute( ATTR_VALUE );
    if ( alt.isEmpty() )
        alt = i18n( "Submit" );

    return alt;
}

bool HTMLInputElementImpl::encoding(const QTextCodec* codec, khtml::encodingList& encoding, bool multipart)
{
    QString nme = name().string();

    // image generates its own name's
    if (nme.isEmpty() && m_type != IMAGE) return false;

    // IMAGE needs special handling later
    if(m_type != IMAGE) encoding += fixUpfromUnicode(codec, nme);

    switch (m_type) {
        case HIDDEN:
        case TEXT:
        case PASSWORD:
            // always successful
            encoding += fixUpfromUnicode(codec, value().string());
            return true;
        case CHECKBOX:

            if( checked() ) {
                encoding += fixUpfromUnicode(codec, value().string());
                return true;
            }
            break;

        case RADIO:

            if( checked() ) {
                encoding += fixUpfromUnicode(codec, value().string());
                return true;
            }
            break;

        case BUTTON:
        case RESET:
            // those buttons are never successful
            return false;

        case IMAGE:

            if(m_clicked && clickX() != -1)
            {
                m_clicked = false;
                QString astr(nme.isEmpty() ? QString::fromLatin1("x") : nme + ".x");

                encoding += fixUpfromUnicode(codec, astr);
                astr.setNum(clickX());
                encoding += fixUpfromUnicode(codec, astr);
                astr = nme.isEmpty() ? QString::fromLatin1("y") : nme + ".y";
                encoding += fixUpfromUnicode(codec, astr);
                astr.setNum(clickY());
                encoding += fixUpfromUnicode(codec, astr);

                return true;
            }
            break;

        case SUBMIT:

            if (m_activeSubmit)
            {
                QString enc_str = m_value.isNull() ?
                    static_cast<RenderSubmitButton*>(m_render)->defaultLabel() : value().string();

                if(!enc_str.isEmpty())
                {
                    encoding += fixUpfromUnicode(codec, enc_str);
                    return true;
                }
            }
            break;

        case FILE:
        {
            // can't submit file on GET
            // don't submit if display: none or display: hidden
            if(!multipart || !renderer() || renderer()->style()->visibility() != khtml::VISIBLE)
                return false;

            QString local;
            QCString dummy("");

            // if no filename at all is entered, return successful, however empty
            // null would be more logical but netscape posts an empty file. argh.
            if(value().isEmpty()) {
                encoding += dummy;
                return true;
            }

            KURL fileurl(value().string());
            KIO::UDSEntry filestat;

            if (!KIO::NetAccess::stat(fileurl, filestat)) {
                KMessageBox::sorry(0L, i18n("Error fetching file for submission:\n%1").arg(KIO::NetAccess::lastErrorString()));
                return false;
            }

            KFileItem fileitem(filestat, fileurl, true, false);
            if(fileitem.isDir()) {
                encoding += dummy;
                return false;
            }

            if ( KIO::NetAccess::download(KURL(value().string()), local) )
            {
                QFile file(local);
                if (file.open(IO_ReadOnly))
                {
                    QCString filearray(file.size()+1);
                    int readbytes = file.readBlock( filearray.data(), file.size());
                    if ( readbytes >= 0 )
                        filearray[readbytes] = '\0';
                    file.close();

                    encoding += filearray;
                    KIO::NetAccess::removeTempFile( local );

                    return true;
                }
                return false;
            }
            else {
                KMessageBox::sorry(0L, i18n("Error fetching file for submission:\n%1").arg(KIO::NetAccess::lastErrorString()));
                return false;
            }
            break;
        }
        case ISINDEX:
            encoding += fixUpfromUnicode(codec, m_value.string());
            return true;
    }
    return false;
}

void HTMLInputElementImpl::reset()
{
    setValue(getAttribute(ATTR_VALUE));
    setChecked(getAttribute(ATTR_CHECKED) != 0);
}

void HTMLInputElementImpl::setChecked(bool _checked)
{
    if (m_form && m_type == RADIO && _checked && !name().isEmpty())
        m_form->radioClicked(this);

    if (m_checked == _checked) return;
    m_checked = _checked;
    setChanged();
}


DOMString HTMLInputElementImpl::value() const
{
    if (m_type == CHECKBOX || m_type == RADIO) {
        if (m_value.isNull() && m_checked)
            return DOMString("on");
        if (!m_checked)
            return DOMString("");
    }

    if(m_value.isNull())
        return DOMString(""); // some JS sites obviously need this
    return m_value;
}


void HTMLInputElementImpl::setValue(DOMString val)
{
    if (m_type == FILE) return;

    m_value = (val.isNull() ? DOMString("") : val);
    setChanged();
}

void HTMLInputElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLInputElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

void HTMLInputElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->isMouseEvent() &&
        ( evt->id() == EventImpl::KHTML_CLICK_EVENT || evt->id() == EventImpl::KHTML_DBLCLICK_EVENT ) &&
        m_type == IMAGE
        && m_render) {
        // record the mouse position for when we get the DOMActivate event
        MouseEventImpl *me = static_cast<MouseEventImpl*>(evt);
        int offsetX, offsetY;
        m_render->absolutePosition(offsetX,offsetY);
        xPos = me->clientX()-offsetX;
        yPos = me->clientY()-offsetY;

	me->setDefaultHandled();
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. Javacsript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if ((evt->id() == EventImpl::DOMACTIVATE_EVENT) &&
        (m_type == IMAGE || m_type == SUBMIT || m_type == RESET)){

        if (!m_form || !m_render)
            return;

        m_clicked = true;
        if (m_type == RESET) {
            m_form->reset();
        }
        else {
            m_activeSubmit = true;
            if (!m_form->prepareSubmit()) {
                xPos = 0;
                yPos = 0;
            }
            m_activeSubmit = false;
        }
    }
    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

bool HTMLInputElementImpl::isEditable()
{
    return ((m_type == TEXT) || (m_type == PASSWORD) || (m_type == ISINDEX) || (m_type == FILE));
}

// -------------------------------------------------------------------------

HTMLLabelElementImpl::HTMLLabelElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLLabelElementImpl::~HTMLLabelElementImpl()
{
}

NodeImpl::Id HTMLLabelElementImpl::id() const
{
    return ID_LABEL;
}

void HTMLLabelElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::FOCUS_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        setHTMLEventListener(EventImpl::BLUR_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

ElementImpl *HTMLLabelElementImpl::formElement()
{
    DOMString formElementId = getAttribute(ATTR_FOR);
    if (formElementId.isNull() || formElementId.isEmpty())
        return 0;
    return getDocument()->getElementById(formElementId);
}

// -------------------------------------------------------------------------

HTMLLegendElementImpl::HTMLLegendElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
}

HTMLLegendElementImpl::~HTMLLegendElementImpl()
{
}

NodeImpl::Id HTMLLegendElementImpl::id() const
{
    return ID_LEGEND;
}

// -------------------------------------------------------------------------

HTMLSelectElementImpl::HTMLSelectElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
    m_multiple = false;
    m_recalcListItems = false;
    // 0 means invalid (i.e. not set)
    m_size = 0;
    m_minwidth = 0;
}

HTMLSelectElementImpl::~HTMLSelectElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
}

NodeImpl::Id HTMLSelectElementImpl::id() const
{
    return ID_SELECT;
}

DOMString HTMLSelectElementImpl::type() const
{
    return (m_multiple ? "select-multiple" : "select-one");
}

long HTMLSelectElementImpl::selectedIndex() const
{
    // return the number of the first option selected
    uint o = 0;
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    for (unsigned int i = 0; i < items.size(); i++) {
        if (items[i]->id() == ID_OPTION) {
            if (static_cast<HTMLOptionElementImpl*>(items[i])->selected())
                return o;
            o++;
        }
    }
    Q_ASSERT(m_multiple);
    return -1;
}

void HTMLSelectElementImpl::setSelectedIndex( long  index )
{
    // deselect all other options and select only the new one
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    int listIndex;
    for (listIndex = 0; listIndex < int(items.size()); listIndex++) {
        if (items[listIndex]->id() == ID_OPTION)
            static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(false);
    }
    listIndex = optionToListIndex(index);
    if (listIndex >= 0)
        static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(true);

    setChanged(true);
}

long HTMLSelectElementImpl::length() const
{
    int len = 0;
    uint i;
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->id() == ID_OPTION)
            len++;
    }
    return len;
}

void HTMLSelectElementImpl::add( const HTMLElement &element, const HTMLElement &before )
{
    if(element.isNull() || element.handle()->id() != ID_OPTION)
        return;

    int exceptioncode = 0;
    insertBefore(element.handle(), before.handle(), exceptioncode );
    if (!exceptioncode)
        setRecalcListItems();
}

void HTMLSelectElementImpl::remove( long index )
{
    int exceptioncode = 0;
    int listIndex = optionToListIndex(index);

    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    if(listIndex < 0 || index >= int(items.size()))
        return; // ### what should we do ? remove the last item?

    removeChild(items[listIndex], exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
}

void HTMLSelectElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLSelectElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

DOMString HTMLSelectElementImpl::value( )
{
    uint i;
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if ( items[i]->id() == ID_OPTION
            && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            return static_cast<HTMLOptionElementImpl*>(items[i])->value();
    }
    return DOMString();
}

void HTMLSelectElementImpl::setValue(DOMStringImpl* /*value*/)
{
    // ### find the option with value() matching the given parameter
    // and make it the current selection.
    kdWarning() << "Unimplemented HTMLSelectElementImpl::setValue called" << endl;
}

QString HTMLSelectElementImpl::state( )
{
#ifndef APPLE_CHANGES
    QString state;
#endif /* APPLE_CHANGES not defined */
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();

    int l = items.count();

#ifdef APPLE_CHANGES
    QChar stateChars[l];
    
    for(int i = 0; i < l; i++)
        if(items[i]->id() == ID_OPTION && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            stateChars[i] = 'X';
        else
            stateChars[i] = '.';
    QString state(stateChars, l);
#else /* APPLE_CHANGES not defined */
    state.fill('.', l);
    for(int i = 0; i < l; i++)
        if(items[i]->id() == ID_OPTION && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            state[i] = 'X';
#endif /* APPLE_CHANGES not defined */

    return state;
}

void HTMLSelectElementImpl::restoreState(const QString &_state)
{
    recalcListItems();

    QString state = _state;
    if(!state.isEmpty() && !state.contains('X') && !m_multiple) {
        qWarning("should not happen in restoreState!");
#ifdef APPLE_CHANGES
        // Invalid access to string's internal buffer.  Should never get here
        // anyway.
        //state[0] = 'X';
#else /* APPLE_CHANGES not defined */
        state[0] = 'X';
#endif /* APPLE_CHANGES not defined */
    }

    QMemArray<HTMLGenericFormElementImpl*> items = listItems();

    int l = items.count();
    for(int i = 0; i < l; i++) {
        if(items[i]->id() == ID_OPTION) {
            HTMLOptionElementImpl* oe = static_cast<HTMLOptionElementImpl*>(items[i]);
            oe->setSelected(state[i] == 'X');
        }
    }
    setChanged(true);
}

NodeImpl *HTMLSelectElementImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::insertBefore(newChild,refChild, exceptioncode );
    if (!exceptioncode)
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::replaceChild(newChild,oldChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::removeChild(oldChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::appendChild(newChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    setChanged(true);
    return result;
}

NodeImpl* HTMLSelectElementImpl::addChild(NodeImpl* newChild)
{
    setRecalcListItems();
    return HTMLGenericFormElementImpl::addChild(newChild);
}

void HTMLSelectElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SIZE:
        m_size = QMAX( attr->val()->toInt(), 1 );
        break;
    case ATTR_WIDTH:
        m_minwidth = QMAX( attr->val()->toInt(), 0 );
        break;
    case ATTR_MULTIPLE:
        m_multiple = (attr->val() != 0);
        break;
    case ATTR_ACCESSKEY:
        // ### ignore for the moment
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::FOCUS_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        setHTMLEventListener(EventImpl::BLUR_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCHANGE:
        setHTMLEventListener(EventImpl::CHANGE_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

void HTMLSelectElementImpl::init()
{
    HTMLGenericFormElementImpl::init();

    addCSSProperty(CSS_PROP_COLOR, "text");
}

void HTMLSelectElementImpl::attach()
{
    assert(!attached());
    assert(parentNode());
    assert(!renderer());

    RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
    _style->ref();
    if (parentNode()->renderer() && _style->display() != NONE) {
        m_render = new RenderSelect(this);
        m_render->setStyle(_style);
    }

    HTMLGenericFormElementImpl::attach();
    _style->deref();
}

bool HTMLSelectElementImpl::encoding(const QTextCodec* codec, khtml::encodingList& encoded_values, bool)
{
    bool successful = false;
    QCString enc_name = fixUpfromUnicode(codec, name().string());
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();

    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->id() == ID_OPTION) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            if (option->selected()) {
                encoded_values += enc_name;
                encoded_values += fixUpfromUnicode(codec, option->value().string());
                successful = true;
            }
        }
    }

    // ### this case should not happen. make sure that we select the first option
    // in any case. otherwise we have no consistency with the DOM interface. FIXME!
    // we return the first one if it was a combobox select
    if (!successful && !m_multiple && m_size <= 1 && items.size() &&
        (items[0]->id() == ID_OPTION) ) {
        HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[0]);
        encoded_values += enc_name;
        if (option->value().isNull())
            encoded_values += fixUpfromUnicode(codec, option->text().string().stripWhiteSpace());
        else
            encoded_values += fixUpfromUnicode(codec, option->value().string());
        successful = true;
    }

    return successful;
}

int HTMLSelectElementImpl::optionToListIndex(int optionIndex) const
{
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    if (optionIndex < 0 || optionIndex >= int(items.size()))
        return -1;

    int listIndex = 0;
    int optionIndex2 = 0;
    for (;
         optionIndex2 < int(items.size()) && optionIndex2 <= optionIndex;
         listIndex++) { // not a typo!
        if (items[listIndex]->id() == ID_OPTION)
            optionIndex2++;
    }
    listIndex--;
    return listIndex;
}

int HTMLSelectElementImpl::listToOptionIndex(int listIndex) const
{
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    if (listIndex < 0 || listIndex >= int(items.size()) ||
        items[listIndex]->id() != ID_OPTION)
        return -1;

    int optionIndex = 0; // actual index of option not counting OPTGROUP entries that may be in list
    int i;
    for (i = 0; i < listIndex; i++)
        if (items[i]->id() == ID_OPTION)
            optionIndex++;
    return optionIndex;
}

void HTMLSelectElementImpl::recalcListItems()
{
    NodeImpl* current = firstChild();
    m_listItems.resize(0);
    HTMLOptionElementImpl* foundSelected = 0;
    while(current) {
        if (current->id() == ID_OPTGROUP && current->firstChild()) {
            // ### what if optgroup contains just comments? don't want one of no options in it...
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLGenericFormElementImpl*>(current);
            current = current->firstChild();
        }
        if (current->id() == ID_OPTION) {
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLGenericFormElementImpl*>(current);
            if (!foundSelected && !m_multiple) {
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
                foundSelected->m_selected = true;
            }
            else if (foundSelected && !m_multiple && static_cast<HTMLOptionElementImpl*>(current)->selected()) {
                foundSelected->m_selected = false;
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
            }
        }
        NodeImpl *parent = current->parentNode();
        current = current->nextSibling();
        if (!current) {
            if (parent != this)
                current = parent->nextSibling();
        }
    }
    m_recalcListItems = false;
}

void HTMLSelectElementImpl::childrenChanged()
{
    setRecalcListItems();

    HTMLGenericFormElementImpl::childrenChanged();
}

void HTMLSelectElementImpl::setRecalcListItems()
{
    m_recalcListItems = true;
    if (m_render)
        static_cast<khtml::RenderSelect*>(m_render)->setOptionsChanged(true);
    setChanged();
}

void HTMLSelectElementImpl::reset()
{
    QMemArray<HTMLGenericFormElementImpl*> items = listItems();
    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->id() == ID_OPTION) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            bool selected = (!option->getAttribute(ATTR_SELECTED).isNull());
            option->setSelected(selected);
        }
    }
    if ( m_render )
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);
    setChanged( true );
}

void HTMLSelectElementImpl::notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected)
{
    if (selected && !m_multiple) {
        // deselect all other options
        QMemArray<HTMLGenericFormElementImpl*> items = listItems();
        uint i;
        for (i = 0; i < items.size(); i++) {
            if (items[i]->id() == ID_OPTION)
                static_cast<HTMLOptionElementImpl*>(items[i])->m_selected = (items[i] == selectedOption);
        }
    }
    if (m_render)
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);

    setChanged(true);
}

// -------------------------------------------------------------------------

HTMLKeygenElementImpl::HTMLKeygenElementImpl(DocumentPtr* doc, HTMLFormElementImpl* f)
    : HTMLSelectElementImpl(doc, f)
{
    QStringList keys = KSSLKeyGen::supportedKeySizes();
    for (QStringList::Iterator i = keys.begin(); i != keys.end(); ++i) {
        HTMLOptionElementImpl* o = new HTMLOptionElementImpl(doc, form());
        addChild(o);
        o->addChild(new TextImpl(doc, DOMString(*i)));
    }
}

NodeImpl::Id HTMLKeygenElementImpl::id() const
{
    return ID_KEYGEN;
}

void HTMLKeygenElementImpl::parseAttribute(AttributeImpl* attr)
{
    switch(attr->id())
    {
    case ATTR_CHALLENGE:
        break;
    default:
        // skip HTMLSelectElementImpl parsing!
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

bool HTMLKeygenElementImpl::encoding(const QTextCodec* codec, khtml::encodingList& encoded_values, bool)
{
    bool successful = false;
    QCString enc_name = fixUpfromUnicode(codec, name().string());

    encoded_values += enc_name;

    // pop up the fancy certificate creation dialog here
    KSSLKeyGen *kg = new KSSLKeyGen(static_cast<RenderWidget *>(m_render)->widget(), "Key Generator", true);

    kg->setKeySize(0);
    successful = (QDialog::Accepted == kg->exec());

    delete kg;

    encoded_values += "deadbeef";

    return successful;
}

// -------------------------------------------------------------------------

HTMLOptGroupElementImpl::HTMLOptGroupElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
}

HTMLOptGroupElementImpl::~HTMLOptGroupElementImpl()
{
}

NodeImpl::Id HTMLOptGroupElementImpl::id() const
{
    return ID_OPTGROUP;
}

NodeImpl *HTMLOptGroupElementImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::insertBefore(newChild,refChild, exceptioncode);
    if ( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::replaceChild(newChild,oldChild, exceptioncode);
    if(!exceptioncode)
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::removeChild(oldChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::appendChild(newChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl* HTMLOptGroupElementImpl::addChild(NodeImpl* newChild)
{
    recalcSelectOptions();

    return HTMLGenericFormElementImpl::addChild(newChild);
}

void HTMLOptGroupElementImpl::parseAttribute(AttributeImpl *attr)
{
    HTMLGenericFormElementImpl::parseAttribute(attr);
    recalcSelectOptions();
}

void HTMLOptGroupElementImpl::recalcSelectOptions()
{
    NodeImpl *select = parentNode();
    while (select && select->id() != ID_SELECT)
        select = select->parentNode();
    if (select)
        static_cast<HTMLSelectElementImpl*>(select)->setRecalcListItems();
}

// -------------------------------------------------------------------------

HTMLOptionElementImpl::HTMLOptionElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
    m_selected = false;
}

NodeImpl::Id HTMLOptionElementImpl::id() const
{
    return ID_OPTION;
}

DOMString HTMLOptionElementImpl::text() const
{
    DOMString label = getAttribute(ATTR_LABEL);
    if (label.isEmpty() && firstChild() && firstChild()->nodeType() == Node::TEXT_NODE) {
	if (firstChild()->nextSibling()) {
	    DOMString ret = "";
	    NodeImpl *n = firstChild();
	    for (; n; n = n->nextSibling()) {
		if (n->nodeType() == Node::TEXT_NODE ||
		    n->nodeType() == Node::CDATA_SECTION_NODE)
		    ret += n->nodeValue();
	    }
	    return ret;
	}
	else
	    return firstChild()->nodeValue();
    }
    else
        return label;
}

long HTMLOptionElementImpl::index() const
{
    // Let's do this dynamically. Might be a bit slow, but we're sure
    // we won't forget to update a member variable in some cases...
    QMemArray<HTMLGenericFormElementImpl*> items = getSelect()->listItems();
    int l = items.count();
    int optionIndex = 0;
    for(int i = 0; i < l; i++) {
        if(items[i]->id() == ID_OPTION)
        {
            if (static_cast<HTMLOptionElementImpl*>(items[i]) == this)
                return optionIndex;
            optionIndex++;
        }
    }
    kdWarning() << "HTMLOptionElementImpl::index(): option not found!" << endl;
    return 0;
}

void HTMLOptionElementImpl::setIndex( long  )
{
    kdWarning() << "Unimplemented HTMLOptionElementImpl::setIndex(long) called" << endl;
    // ###
}

void HTMLOptionElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SELECTED:
        m_selected = (attr->val() != 0);
        break;
    case ATTR_VALUE:
        m_value = attr->value();
        break;
    default:
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

DOMString HTMLOptionElementImpl::value() const
{
    if ( !m_value.isNull() )
        return m_value;
    // Use the text if the value wasn't set.
    return text().string().stripWhiteSpace();
}

void HTMLOptionElementImpl::setValue(DOMStringImpl* value)
{
    setAttribute(ATTR_VALUE, value);
}

void HTMLOptionElementImpl::setSelected(bool _selected)
{
    if(m_selected == _selected)
        return;
    m_selected = _selected;
    HTMLSelectElementImpl *select = getSelect();
    if (select)
        select->notifyOptionSelected(this,_selected);
}

HTMLSelectElementImpl *HTMLOptionElementImpl::getSelect() const
{
    NodeImpl *select = parentNode();
    while (select && select->id() != ID_SELECT)
        select = select->parentNode();
    return static_cast<HTMLSelectElementImpl*>(select);
}

// -------------------------------------------------------------------------

HTMLTextAreaElementImpl::HTMLTextAreaElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(doc, f)
{
    // DTD requires rows & cols be specified, but we will provide reasonable defaults
    m_rows = 2;
    m_cols = 20;
    m_wrap = ta_Virtual;
    m_dirtyvalue = true;
}

HTMLTextAreaElementImpl::~HTMLTextAreaElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
}

NodeImpl::Id HTMLTextAreaElementImpl::id() const
{
    return ID_TEXTAREA;
}

DOMString HTMLTextAreaElementImpl::type() const
{
    return "textarea";
}

QString HTMLTextAreaElementImpl::state( )
{
    // Make sure the string is not empty!
    return value().string()+'.';
}

void HTMLTextAreaElementImpl::restoreState(const QString &state)
{
    setDefaultValue(state.left(state.length()-1));
    // the close() in the rendertree will take care of transferring defaultvalue to 'value'
}

void HTMLTextAreaElementImpl::select(  )
{
    if (m_render)
        static_cast<RenderTextArea*>(m_render)->select();
    onSelect();
}

void HTMLTextAreaElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ROWS:
        m_rows = attr->val() ? attr->val()->toInt() : 3;
        break;
    case ATTR_COLS:
        m_cols = attr->val() ? attr->val()->toInt() : 60;
        break;
    case ATTR_WRAP:
        // virtual / physical is Netscape extension of HTML 3.0, now deprecated
        // soft/ hard / off is recommendation for HTML 4 extension by IE and NS 4
        if ( strcasecmp( attr->value(), "virtual" ) == 0  || strcasecmp( attr->value(), "soft") == 0)
            m_wrap = ta_Virtual;
        else if ( strcasecmp ( attr->value(), "physical" ) == 0 || strcasecmp( attr->value(), "hard") == 0)
            m_wrap = ta_Physical;
        else if(strcasecmp( attr->value(), "on" ) == 0)
            m_wrap = ta_Physical;
        else if(strcasecmp( attr->value(), "off") == 0)
            m_wrap = ta_NoWrap;
        break;
    case ATTR_ACCESSKEY:
        // ignore for the moment
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::FOCUS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        setHTMLEventListener(EventImpl::BLUR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONSELECT:
        setHTMLEventListener(EventImpl::SELECT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCHANGE:
        setHTMLEventListener(EventImpl::CHANGE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

void HTMLTextAreaElementImpl::init()
{
    HTMLGenericFormElementImpl::init();

    addCSSProperty(CSS_PROP_COLOR, "text");
}

void HTMLTextAreaElementImpl::attach()
{
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

    RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
    _style->ref();
    if (parentNode()->renderer() && _style->display() != NONE) {
        m_render = new RenderTextArea(this);
        m_render->setStyle(_style);
    }

    HTMLGenericFormElementImpl::attach();
    _style->deref();
}

bool HTMLTextAreaElementImpl::encoding(const QTextCodec* codec, encodingList& encoding, bool)
{
    if (name().isEmpty() || !m_render) return false;

    encoding += fixUpfromUnicode(codec, name().string());
    encoding += fixUpfromUnicode(codec, value().string());

    return true;
}

void HTMLTextAreaElementImpl::reset()
{
    setValue(defaultValue());
}

DOMString HTMLTextAreaElementImpl::value()
{
    if ( m_dirtyvalue) {
        if ( m_render )  m_value = static_cast<RenderTextArea*>( m_render )->text();
        m_dirtyvalue = false;
    }

    if ( m_value.isNull() ) return "";

    return m_value;
}

void HTMLTextAreaElementImpl::setValue(DOMString _value)
{
    m_value = _value.string();
    m_dirtyvalue = false;
    setChanged(true);
}


DOMString HTMLTextAreaElementImpl::defaultValue()
{
    DOMString val = "";
    // there may be comments - just grab the text nodes
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            val += static_cast<TextImpl*>(n)->data();
    if (val[0] == '\r' && val[1] == '\n') {
	val = val.copy();
	val.remove(0,2);
    }
    else if (val[0] == '\r' || val[0] == '\n') {
	val = val.copy();
	val.remove(0,1);
    }

    return val;
}

void HTMLTextAreaElementImpl::setDefaultValue(DOMString _defaultValue)
{
    // there may be comments - remove all the text nodes and replace them with one
    QPtrList<NodeImpl> toRemove;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            toRemove.append(n);
    QPtrListIterator<NodeImpl> it(toRemove);
    int exceptioncode = 0;
    for (; it.current(); ++it) {
        removeChild(it.current(), exceptioncode);
    }
    insertBefore(getDocument()->createTextNode(_defaultValue),firstChild(), exceptioncode);
    setValue(_defaultValue);
}

void HTMLTextAreaElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLTextAreaElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

bool HTMLTextAreaElementImpl::isEditable()
{
    return true;
}

// -------------------------------------------------------------------------

HTMLIsIndexElementImpl::HTMLIsIndexElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLInputElementImpl(doc, f)
{
    m_type = TEXT;
    setName("isindex");
}

HTMLIsIndexElementImpl::~HTMLIsIndexElementImpl()
{
}

NodeImpl::Id HTMLIsIndexElementImpl::id() const
{
    return ID_ISINDEX;
}

void HTMLIsIndexElementImpl::parseAttribute(AttributeImpl* attr)
{
    switch(attr->id())
    {
    case ATTR_PROMPT:
	setValue(attr->value());
    default:
        // don't call HTMLInputElement::parseAttribute here, as it would
        // accept attributes this element does not support
        HTMLGenericFormElementImpl::parseAttribute(attr);
    }
}

// -------------------------------------------------------------------------

