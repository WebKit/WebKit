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
 * Copyright © World Wide Web Consortium , (Massachusetts Institute of
 * Technology , Institut National de Recherche en Informatique et en
 * Automatique , Keio University ). All Rights Reserved.
 *
 */
#ifndef HTML_FORM_H
#define HTML_FORM_H

// --------------------------------------------------------------------------
#include <dom/html_element.h>
#include <dom/html_misc.h>

namespace DOM {

class HTMLButtonElementImpl;
class HTMLFormElement;
class DOMString;

/**
 * Push button. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-BUTTON">
 * BUTTON element definition </a> in HTML 4.0.
 *
 */
class HTMLButtonElement : public HTMLElement
{
public:
    HTMLButtonElement();
    HTMLButtonElement(const HTMLButtonElement &other);
    HTMLButtonElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLButtonElement(HTMLButtonElementImpl *impl);
public:

    HTMLButtonElement & operator = (const HTMLButtonElement &other);
    HTMLButtonElement & operator = (const Node &other);

    ~HTMLButtonElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Form control or object name when submitted with a form. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-name-BUTTON">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );
    
    /**
     * Index that represents the element's position in the tabbing
     * order. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-tabindex">
     * tabindex attribute definition </a> in HTML 4.0.
     *
     */
    long tabIndex() const;

    /**
     * see @ref tabIndex
     */
    void setTabIndex( long );

    /**
     * The type of button. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-type-BUTTON">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * The current form control value. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-value-BUTTON">
     * value attribute definition </a> in HTML 4.0.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );
    
    /**
     * Removes keyboard focus from this element.
     *
     */
    void blur (  );
        
    /**
     * Gives keyboard focus to this element.
     *
     */
    void focus (  );
};
// --------------------------------------------------------------------------

class HTMLFieldSetElementImpl;
/**
 * Organizes form controls into logical groups. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-FIELDSET">
 * FIELDSET element definition </a> in HTML 4.0.
 *
 */
class HTMLFieldSetElement : public HTMLElement
{
public:
    HTMLFieldSetElement();
    HTMLFieldSetElement(const HTMLFieldSetElement &other);
    HTMLFieldSetElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLFieldSetElement(HTMLFieldSetElementImpl *impl);
public:

    HTMLFieldSetElement & operator = (const HTMLFieldSetElement &other);
    HTMLFieldSetElement & operator = (const Node &other);

    ~HTMLFieldSetElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;
};

// --------------------------------------------------------------------------

class HTMLFormElementImpl;
/**
 * The <code> FORM </code> element encompasses behavior similar to a
 * collection and an element. It provides direct access to the
 * contained input elements as well as the attributes of the form
 * element. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-FORM">
 * FORM element definition </a> in HTML 4.0.
 *
 */
class HTMLFormElement : public HTMLElement
{
    friend class HTMLButtonElement;
    friend class HTMLFieldSetElement;
    friend class HTMLInputElement;
    friend class HTMLLabelElement;
    friend class HTMLLegendElement;
    friend class HTMLSelectElement;
    friend class HTMLTextAreaElement;
    friend class HTMLOptionElement;
    friend class HTMLIsIndexElement;
    friend class HTMLObjectElement;

public:
    HTMLFormElement();
    HTMLFormElement(const HTMLFormElement &other);
    HTMLFormElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLFormElement(HTMLFormElementImpl *impl);
public:

    HTMLFormElement & operator = (const HTMLFormElement &other);
    HTMLFormElement & operator = (const Node &other);

    ~HTMLFormElement();

    /**
     * Returns a collection of all control elements in the form.
     *
     */
    HTMLCollection elements() const;

    /**
     * The number of form controls in the form.
     *
     */
    long length() const;

    /**
     * Names the form.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * List of character sets supported by the server. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accept-charset">
     * accept-charset attribute definition </a> in HTML 4.0.
     *
     */
    DOMString acceptCharset() const;

    /**
     * see @ref acceptCharset
     */
    void setAcceptCharset( const DOMString & );

    /**
     * Server-side form handler. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-action">
     * action attribute definition </a> in HTML 4.0.
     *
     */
    DOMString action() const;

    /**
     * see @ref action
     */
    void setAction( const DOMString & );

    /**
     * The content type of the submitted form, generally
     * "application/x-www-form-urlencoded". See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-enctype">
     * enctype attribute definition </a> in HTML 4.0.
     *
     */
    DOMString enctype() const;

    /**
     * see @ref enctype
     */
    void setEnctype( const DOMString & );

    /**
     * HTTP method used to submit form. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-method">
     * method attribute definition </a> in HTML 4.0.
     *
     */
    DOMString method() const;

    /**
     * see @ref method
     */
    void setMethod( const DOMString & );

    /**
     * Frame to render the resource in. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-target">
     * target attribute definition </a> in HTML 4.0.
     *
     */
    DOMString target() const;

    /**
     * see @ref target
     */
    void setTarget( const DOMString & );

    /**
     * Submits the form. It performs the same action as a submit
     * button.
     *
     */
    void submit (  );

    /**
     * Restores a form element's default values. It performs the same
     * action as a reset button.
     *
     */
    void reset (  );
};

// --------------------------------------------------------------------------

class HTMLInputElementImpl;
/**
 * Form control. Note. Depending upon the environment the page is
 * being viewed, the value property may be read-only for the file
 * upload input type. For the "password" input type, the actual value
 * returned may be masked to prevent unauthorized use. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-INPUT">
 * INPUT element definition </a> in HTML 4.0.
 *
 */
class HTMLInputElement : public HTMLElement
{
public:
    HTMLInputElement();
    HTMLInputElement(const HTMLInputElement &other);
    HTMLInputElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLInputElement(HTMLInputElementImpl *impl);
public:

    HTMLInputElement & operator = (const HTMLInputElement &other);
    HTMLInputElement & operator = (const Node &other);

    ~HTMLInputElement();

    /**
     * Stores the initial control value (i.e., the initial value of
     * <code> value </code> ).
     *
     */
    DOMString defaultValue() const;

    /**
     * see @ref defaultValue
     */
    void setDefaultValue( const DOMString & );

    /**
     * When <code> type </code> has the value "Radio" or "Checkbox",
     * stores the initial value of the <code> checked </code>
     * attribute.
     *
     */
    bool defaultChecked() const;

    /**
     * see @ref defaultChecked
     */
    void setDefaultChecked( bool );

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * A comma-separated list of content types that a server
     * processing this form will handle correctly. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accept">
     * accept attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accept() const;

    /**
     * see @ref accept
     */
    void setAccept( const DOMString & );

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * Aligns this object (vertically or horizontally) with respect to
     * its surrounding text. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-align-IMG">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );

    /**
     * Alternate text for user agents not rendering the normal content
     * of this element. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-alt">
     * alt attribute definition </a> in HTML 4.0.
     *
     */
    DOMString alt() const;

    /**
     * see @ref alt
     */
    void setAlt( const DOMString & );

    /**
     * Describes whether a radio or check box is checked, when <code>
     * type </code> has the value "Radio" or "Checkbox". The value is
     * TRUE if explicitly set. Represents the current state of the
     * checkbox or radio button. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-checked">
     * checked attribute definition </a> in HTML 4.0.
     *
     */
    bool checked() const;

    /**
     * see @ref checked
     */
    void setChecked( bool );

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Maximum number of characters for text fields, when <code> type
     * </code> has the value "Text" or "Password". See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-maxlength">
     * maxlength attribute definition </a> in HTML 4.0.
     *
     */
    long maxLength() const;

    /**
     * see @ref maxLength
     */
    void setMaxLength( long );

    /**
     * Form control or object name when submitted with a form. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-name-INPUT">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * This control is read-only. When <code> type </code> has the
     * value "text" or "password" only. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-readonly">
     * readonly attribute definition </a> in HTML 4.0.
     *
     */
    bool readOnly() const;

    /**
     * see @ref readOnly
     */
    void setReadOnly( bool );

    /**
     * Size information. The precise meaning is specific to each type
     * of field. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-size-INPUT">
     * size attribute definition </a> in HTML 4.0.
     *
     */
    DOMString size() const;

    /**
     * see @ref size
     */
    void setSize( const DOMString & );

    /**
     * When the <code> type </code> attribute has the value "Image",
     * this attribute specifies the location of the image to be used
     * to decorate the graphical submit button. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-src">
     * src attribute definition </a> in HTML 4.0.
     *
     */
    DOMString src() const;

    /**
     * see @ref src
     */
    void setSrc( const DOMString & );

    /**
     * Index that represents the element's position in the tabbing
     * order. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-tabindex">
     * tabindex attribute definition </a> in HTML 4.0.
     *
     */
    long tabIndex() const;

    /**
     * see @ref tabIndex
     */
    void setTabIndex( long );

    /**
     * The type of control created. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-type-INPUT">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType(const DOMString&);

    /**
     * Use client-side image map. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-usemap">
     * usemap attribute definition </a> in HTML 4.0.
     *
     */
    DOMString useMap() const;

    /**
     * see @ref useMap
     */
    void setUseMap( const DOMString & );

    /**
     * The current form control value. Used for radio buttons and
     * check boxes. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-value-INPUT">
     * value attribute definition </a> in HTML 4.0.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );

    /**
     * Removes keyboard focus from this element.
     *
     */
    void blur (  );

    /**
     * Gives keyboard focus to this element.
     *
     */
    void focus (  );

    /**
     * Select the contents of the text area. For <code> INPUT </code>
     * elements whose <code> type </code> attribute has one of the
     * following values: "Text", "File", or "Password".
     *
     */
    void select (  );

    /**
     * Simulate a mouse-click. For <code> INPUT </code> elements whose
     * <code> type </code> attribute has one of the following values:
     * "Button", "Checkbox", "Radio", "Reset", or "Submit".
     */
    void click (  );
};

// --------------------------------------------------------------------------

class HTMLLabelElementImpl;
/**
 * Form field label text. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-LABEL">
 * LABEL element definition </a> in HTML 4.0.
 *
 */
class HTMLLabelElement : public HTMLElement
{
public:
    HTMLLabelElement();
    HTMLLabelElement(const HTMLLabelElement &other);
    HTMLLabelElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLLabelElement(HTMLLabelElementImpl *impl);
public:

    HTMLLabelElement & operator = (const HTMLLabelElement &other);
    HTMLLabelElement & operator = (const Node &other);

    ~HTMLLabelElement();

    // not part of the DOM.
    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     * deprecated - don't use. Provided for KDE2 compatibility only.
     */
    HTMLFormElement form() const;

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * This attribute links this label with another form control by
     * <code> id </code> attribute. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-for">
     * for attribute definition </a> in HTML 4.0.
     *
     */
    DOMString htmlFor() const;

    /**
     * see @ref htmlFor
     */
    void setHtmlFor( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLLegendElementImpl;
/**
 * Provides a caption for a <code> FIELDSET </code> grouping. See the
 * <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-LEGEND">
 * LEGEND element definition </a> in HTML 4.0.
 *
 */
class HTMLLegendElement : public HTMLElement
{
public:
    HTMLLegendElement();
    HTMLLegendElement(const HTMLLegendElement &other);
    HTMLLegendElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLLegendElement(HTMLLegendElementImpl *impl);
public:

    HTMLLegendElement & operator = (const HTMLLegendElement &other);
    HTMLLegendElement & operator = (const Node &other);

    ~HTMLLegendElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * Text alignment relative to <code> FIELDSET </code> . See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-align-LEGEND">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLOptGroupElementImpl;
/**
 * Group options together in logical subdivisions. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-OPTGROUP">
 * OPTGROUP element definition </a> in HTML 4.0.
 *
 */
class HTMLOptGroupElement : public HTMLElement
{
public:
    HTMLOptGroupElement();
    HTMLOptGroupElement(const HTMLOptGroupElement &other);
    HTMLOptGroupElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLOptGroupElement(HTMLOptGroupElementImpl *impl);
public:

    HTMLOptGroupElement & operator = (const HTMLOptGroupElement &other);
    HTMLOptGroupElement & operator = (const Node &other);

    ~HTMLOptGroupElement();

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Assigns a label to this option group. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-label-OPTGROUP">
     * label attribute definition </a> in HTML 4.0.
     *
     */
    DOMString label() const;

    /**
     * see @ref label
     */
    void setLabel( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLSelectElementImpl;
/**
 * The select element allows the selection of an option. The contained
 * options can be directly accessed through the select element as a
 * collection. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-SELECT">
 * SELECT element definition </a> in HTML 4.0.
 *
 */
class HTMLSelectElement : public HTMLElement
{
public:
    HTMLSelectElement();
    HTMLSelectElement(const HTMLSelectElement &other);
    HTMLSelectElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLSelectElement(HTMLSelectElementImpl *impl);
public:

    HTMLSelectElement & operator = (const HTMLSelectElement &other);
    HTMLSelectElement & operator = (const Node &other);

    ~HTMLSelectElement();

    /**
     * The type of control created.
     *
     */
    DOMString type() const;

    /**
     * The ordinal index of the selected option. The value -1 is
     * returned if no element is selected. If multiple options are
     * selected, the index of the first selected option is returned.
     *
     */
    long selectedIndex() const;

    /**
     * see @ref selectedIndex
     */
    void setSelectedIndex( long );

    /**
     * The current form control value.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );

    /**
     * The number of options in this <code> SELECT </code> .
     *
     */
    long length() const;

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * The collection of <code> OPTION </code> elements contained by
     * this element.
     *
     */
    HTMLCollection options() const;

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * If true, multiple <code> OPTION </code> elements may be
     * selected in this <code> SELECT </code> . See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-multiple">
     * multiple attribute definition </a> in HTML 4.0.
     *
     */
    bool multiple() const;

    /**
     * see @ref multiple
     */
    void setMultiple( bool );

    /**
     * Form control or object name when submitted with a form. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-name-SELECT">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Number of visible rows. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-size-SELECT">
     * size attribute definition </a> in HTML 4.0.
     *
     */
    long size() const;

    /**
     * see @ref size
     */
    void setSize( long );

    /**
     * Index that represents the element's position in the tabbing
     * order. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-tabindex">
     * tabindex attribute definition </a> in HTML 4.0.
     *
     */
    long tabIndex() const;

    /**
     * see @ref tabIndex
     */
    void setTabIndex( long );

    /**
     * Add a new element to the collection of <code> OPTION </code>
     * elements for this <code> SELECT </code> .
     *
     * @param element The element to add.
     *
     * @param before The element to insert before, or 0 for the
     * tail of the list.
     *
     */
    void add ( const HTMLElement &element, const HTMLElement &before );

    /**
     * Remove an element from the collection of <code> OPTION </code>
     * elements for this <code> SELECT </code> . Does nothing if no
     * element has the given index.
     *
     * @param index The index of the item to remove.
     *
     */
    void remove ( long index );

    /**
     * Removes keyboard focus from this element.
     *
     */
    void blur (  );

    /**
     * Gives keyboard focus to this element.
     *
     */
    void focus (  );
};

// --------------------------------------------------------------------------

class HTMLTextAreaElementImpl;
/**
 * Multi-line text field. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-TEXTAREA">
 * TEXTAREA element definition </a> in HTML 4.0.
 *
 */
class HTMLTextAreaElement : public HTMLElement
{
public:
    HTMLTextAreaElement();
    HTMLTextAreaElement(const HTMLTextAreaElement &other);
    HTMLTextAreaElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLTextAreaElement(HTMLTextAreaElementImpl *impl);
public:

    HTMLTextAreaElement & operator = (const HTMLTextAreaElement &other);
    HTMLTextAreaElement & operator = (const Node &other);

    ~HTMLTextAreaElement();

    /**
     * Stores the initial control value (i.e., the initial value of
     * <code> value </code> ).
     *
     */
    DOMString defaultValue() const;

    /**
     * see @ref defaultValue
     */
    void setDefaultValue( const DOMString & );

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * Width of control (in characters). See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-cols-TEXTAREA">
     * cols attribute definition </a> in HTML 4.0.
     *
     */
    long cols() const;

    /**
     * see @ref cols
     */
    void setCols( long );

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Form control or object name when submitted with a form. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-name-TEXTAREA">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * This control is read-only. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-readonly">
     * readonly attribute definition </a> in HTML 4.0.
     *
     */
    bool readOnly() const;

    /**
     * see @ref readOnly
     */
    void setReadOnly( bool );

    /**
     * Number of text rows. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-rows-TEXTAREA">
     * rows attribute definition </a> in HTML 4.0.
     *
     */
    long rows() const;

    /**
     * see @ref rows
     */
    void setRows( long );

    /**
     * Index that represents the element's position in the tabbing
     * order. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-tabindex">
     * tabindex attribute definition </a> in HTML 4.0.
     *
     */
    long tabIndex() const;

    /**
     * see @ref tabIndex
     */
    void setTabIndex( long );

    /**
     * The type of this form control.
     *
     */
    DOMString type() const;

    /**
     * The current textual content of the multi-line text field. If
     * the entirety of the data can not fit into a single wstring, the
     * implementation may truncate the data.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );

    /**
     * Removes keyboard focus from this element.
     */
    void blur (  );

    /**
     * Gives keyboard focus to this element.
     */
    void focus (  );

    /**
     * Select the contents of the <code> TEXTAREA </code> .
     */
    void select (  );
};

// --------------------------------------------------------------------------

class HTMLOptionElementImpl;
/**
 * A selectable choice. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-OPTION">
 * OPTION element definition </a> in HTML 4.0.
 *
 */
class HTMLOptionElement : public HTMLElement
{
public:
    HTMLOptionElement();
    HTMLOptionElement(const HTMLOptionElement &other);
    HTMLOptionElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLOptionElement(HTMLOptionElementImpl *impl);
public:

    HTMLOptionElement & operator = (const HTMLOptionElement &other);
    HTMLOptionElement & operator = (const Node &other);

    ~HTMLOptionElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * Stores the initial value of the <code> selected </code>
     * attribute.
     *
     */
    bool defaultSelected() const;

    /**
     * see @ref defaultSelected
     */
    void setDefaultSelected( bool );

    /**
     * The text contained within the option element.
     *
     */
    DOMString text() const;

    /**
     * The index of this <code> OPTION </code> in its parent <code>
     * SELECT </code> .
     *
     */
    long index() const;

    /**
     * see @ref index
     *
     * This function is obsolete - the index property is actually supposed to be read-only
     * (http://www.w3.org/DOM/updates/REC-DOM-Level-1-19981001-errata.html)
     */
    void setIndex( long );

    /**
     * The control is unavailable in this context. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-disabled">
     * disabled attribute definition </a> in HTML 4.0.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Option label for use in hierarchical menus. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-label-OPTION">
     * label attribute definition </a> in HTML 4.0.
     *
     */
    DOMString label() const;

    /**
     * see @ref label
     */
    void setLabel( const DOMString & );

    /**
     * Means that this option is initially selected. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-selected">
     * selected attribute definition </a> in HTML 4.0.
     *
     */
    bool selected() const;

    /**
     * see @ref selected
     */
    void setSelected( bool );

    /**
     * The current form control value. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-value-OPTION">
     * value attribute definition </a> in HTML 4.0.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );
};


// --------------------------------------------------------------------------

class HTMLIsIndexElementImpl;
class HTMLFormElement;

/**
 * This element is used for single-line text input. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/forms.html#edef-ISINDEX">
 * ISINDEX element definition </a> in HTML 4.0. This element is
 * deprecated in HTML 4.0.
 *
 */
class HTMLIsIndexElement : public HTMLElement
{
public:
    HTMLIsIndexElement();
    HTMLIsIndexElement(const HTMLIsIndexElement &other);
    HTMLIsIndexElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLIsIndexElement(HTMLIsIndexElementImpl *impl);
public:

    HTMLIsIndexElement & operator = (const HTMLIsIndexElement &other);
    HTMLIsIndexElement & operator = (const Node &other);

    ~HTMLIsIndexElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * The prompt message. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-prompt">
     * prompt attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString prompt() const;

    /**
     * see @ref prompt
     */
    void setPrompt( const DOMString & );
};

}; //namespace

#endif
