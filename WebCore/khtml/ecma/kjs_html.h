// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _KJS_HTML_H_
#define _KJS_HTML_H_

#include "kjs_dom.h"

#include <qguardedptr.h>
#include "misc/loader_client.h"

#include <ApplicationServices/ApplicationServices.h>

namespace DOM {
    class HTMLCollectionImpl;
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class HTMLImageElementImpl;
    class HTMLSelectElementImpl;
    class HTMLTableCaptionElementImpl;
    class HTMLTableSectionElementImpl;
};

namespace KJS {

  class JSAbstractEventListener;

  class HTMLDocument : public DOMDocument {
  public:
    HTMLDocument(ExecState *exec, DOM::HTMLDocumentImpl *d);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Title, Referrer, Domain, URL, Body, Location, Cookie,
           Images, Applets, Embeds, Links, Forms, Anchors, Scripts, All, Clear, Open, Close,
           Write, WriteLn, GetElementsByName, CaptureEvents, ReleaseEvents,
           BgColor, FgColor, AlinkColor, LinkColor, VlinkColor, LastModified, Height, Width, Dir, DesignMode };
  private:
    static ValueImp *namedItemGetter(ExecState *, const Identifier&, const PropertySlot&);
  };

  class HTMLElement : public DOMElement {
  public:
    HTMLElement(ExecState *exec, DOM::HTMLElementImpl *e);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int);
    virtual UString toString(ExecState *exec) const;
    virtual void pushEventHandlerScope(ExecState *exec, ScopeChain &scope) const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List&args);
    virtual bool implementsCall() const;
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;

    static const ClassInfo html_info, head_info, link_info, title_info,
      meta_info, base_info, isIndex_info, style_info, body_info, form_info,
      select_info, optGroup_info, option_info, input_info, textArea_info,
      button_info, label_info, fieldSet_info, legend_info, ul_info, ol_info,
      dl_info, dir_info, menu_info, li_info, div_info, p_info, heading_info,
      blockQuote_info, q_info, pre_info, br_info, baseFont_info, font_info,
      hr_info, mod_info, a_info, canvas_info, img_info, object_info, param_info,
      applet_info, map_info, area_info, script_info, table_info,
      caption_info, col_info, tablesection_info, tr_info,
      tablecell_info, frameSet_info, frame_info, iFrame_info, marquee_info;

    // FIXME: Might make sense to combine this with ClassInfo some day.
    typedef ValueImp *(HTMLElement::*GetterFunction)(ExecState *exec, int token) const;
    typedef void (HTMLElement::*SetterFunction)(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    struct Accessors { GetterFunction m_getter; SetterFunction m_setter; };
    const Accessors* getSetInfo() const;
    static const Accessors html_accessors, head_accessors, link_accessors, title_accessors,
      meta_accessors, base_accessors, isIndex_accessors, style_accessors, body_accessors, form_accessors,
      select_accessors, optGroup_accessors, option_accessors, input_accessors, textArea_accessors,
      button_accessors, label_accessors, fieldSet_accessors, legend_accessors, ul_accessors, ol_accessors,
      dl_accessors, dir_accessors, menu_accessors, li_accessors, div_accessors, p_accessors, heading_accessors,
      blockQuote_accessors, q_accessors, pre_accessors, br_accessors, baseFont_accessors, font_accessors,
      hr_accessors, mod_accessors, a_accessors, canvas_accessors, img_accessors, object_accessors, param_accessors,
      applet_accessors, map_accessors, area_accessors, script_accessors, table_accessors,
      caption_accessors, col_accessors, tablesection_accessors, tr_accessors,
      tablecell_accessors, frameSet_accessors, frame_accessors, iFrame_accessors, marquee_accessors;

    ValueImp *htmlGetter(ExecState* exec, int token) const;
    void  htmlSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *headGetter(ExecState* exec, int token) const;
    void  headSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *linkGetter(ExecState* exec, int token) const;
    void  linkSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *titleGetter(ExecState* exec, int token) const;
    void  titleSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *metaGetter(ExecState* exec, int token) const;
    void  metaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *baseGetter(ExecState* exec, int token) const;
    void  baseSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *isIndexGetter(ExecState* exec, int token) const;
    void  isIndexSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *styleGetter(ExecState* exec, int token) const;
    void  styleSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *bodyGetter(ExecState* exec, int token) const;
    void  bodySetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *formGetter(ExecState* exec, int token) const;
    void  formSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *selectGetter(ExecState* exec, int token) const;
    void  selectSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *optGroupGetter(ExecState* exec, int token) const;
    void  optGroupSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *optionGetter(ExecState* exec, int token) const;
    void  optionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *inputGetter(ExecState* exec, int token) const;
    void  inputSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *textAreaGetter(ExecState* exec, int token) const;
    void  textAreaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *buttonGetter(ExecState* exec, int token) const;
    void  buttonSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *labelGetter(ExecState* exec, int token) const;
    void  labelSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *fieldSetGetter(ExecState* exec, int token) const;
    void  fieldSetSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *legendGetter(ExecState* exec, int token) const;
    void  legendSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *uListGetter(ExecState* exec, int token) const;
    void  uListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *oListGetter(ExecState* exec, int token) const;
    void  oListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *dListGetter(ExecState* exec, int token) const;
    void  dListSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *dirGetter(ExecState* exec, int token) const;
    void  dirSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *menuGetter(ExecState* exec, int token) const;
    void  menuSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *liGetter(ExecState* exec, int token) const;
    void  liSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *divGetter(ExecState* exec, int token) const;
    void  divSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *paragraphGetter(ExecState* exec, int token) const;
    void  paragraphSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *headingGetter(ExecState* exec, int token) const;
    void  headingSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *blockQuoteGetter(ExecState* exec, int token) const;
    void  blockQuoteSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *quoteGetter(ExecState* exec, int token) const;
    void  quoteSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *preGetter(ExecState* exec, int token) const;
    void  preSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *brGetter(ExecState* exec, int token) const;
    void  brSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *baseFontGetter(ExecState* exec, int token) const;
    void  baseFontSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *fontGetter(ExecState* exec, int token) const;
    void  fontSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *hrGetter(ExecState* exec, int token) const;
    void  hrSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *modGetter(ExecState* exec, int token) const;
    void  modSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *anchorGetter(ExecState* exec, int token) const;
    void  anchorSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *imageGetter(ExecState* exec, int token) const;
    void  imageSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *objectGetter(ExecState* exec, int token) const;
    void  objectSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *paramGetter(ExecState* exec, int token) const;
    void  paramSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *appletGetter(ExecState* exec, int token) const;
    void  appletSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *mapGetter(ExecState* exec, int token) const;
    void  mapSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *areaGetter(ExecState* exec, int token) const;
    void  areaSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *scriptGetter(ExecState* exec, int token) const;
    void  scriptSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableGetter(ExecState* exec, int token) const;
    void  tableSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableCaptionGetter(ExecState* exec, int token) const;
    void  tableCaptionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableColGetter(ExecState* exec, int token) const;
    void  tableColSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableSectionGetter(ExecState* exec, int token) const;
    void  tableSectionSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableRowGetter(ExecState* exec, int token) const;
    void  tableRowSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *tableCellGetter(ExecState* exec, int token) const;
    void  tableCellSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *frameSetGetter(ExecState* exec, int token) const;
    void  frameSetSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *frameGetter(ExecState* exec, int token) const;
    void  frameSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *iFrameGetter(ExecState* exec, int token) const;
    void  iFrameSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);
    ValueImp *marqueeGetter(ExecState* exec, int token) const;
    void  marqueeSetter(ExecState *exec, int token, ValueImp *value, const DOM::DOMString& str);

    enum { HtmlVersion, HeadProfile, LinkHref, LinkRel, LinkMedia,
           LinkCharset, LinkDisabled, LinkHrefLang, LinkRev, LinkTarget, LinkType,
           LinkSheet, TitleText, MetaName, MetaHttpEquiv, MetaContent, MetaScheme,
           BaseHref, BaseTarget, IsIndexForm, IsIndexPrompt, StyleDisabled,
           StyleSheet, StyleType, StyleMedia, BodyBackground, BodyVLink, BodyText,
           BodyLink, BodyALink, BodyBgColor, BodyScrollLeft, BodyScrollTop, BodyScrollHeight, BodyScrollWidth,
           FormAction, FormEncType, FormElements, FormLength, FormAcceptCharset,
           FormReset, FormTarget, FormName, FormMethod, FormSubmit, SelectAdd,
           SelectTabIndex, SelectValue, SelectSelectedIndex, SelectLength,
           SelectRemove, SelectForm, SelectBlur, SelectType, SelectOptions,
           SelectDisabled, SelectMultiple, SelectName, SelectSize, SelectFocus,
           OptGroupDisabled, OptGroupLabel, OptionIndex, OptionSelected,
           OptionForm, OptionText, OptionDefaultSelected, OptionDisabled,
           OptionLabel, OptionValue, InputBlur, InputReadOnly, InputAccept,
           InputSize, InputDefaultValue, InputTabIndex, InputValue, InputType,
           InputFocus, InputMaxLength, InputDefaultChecked, InputDisabled,
           InputChecked, InputIndeterminate, InputForm, InputAccessKey, InputAlign, InputAlt,
           InputName, InputSrc, InputUseMap, InputSelect, InputClick,
           InputSelectionStart, InputSelectionEnd, InputSetSelectionRange,
           TextAreaAccessKey, TextAreaName, TextAreaDefaultValue, TextAreaSelect, TextAreaSetSelectionRange,
           TextAreaCols, TextAreaDisabled, TextAreaForm, TextAreaType,
           TextAreaTabIndex, TextAreaReadOnly, TextAreaRows, TextAreaValue,
           TextAreaSelectionStart, TextAreaSelectionEnd,
           TextAreaBlur, TextAreaFocus, ButtonBlur, ButtonFocus, ButtonForm, ButtonTabIndex, ButtonName,
           ButtonDisabled, ButtonAccessKey, ButtonType, ButtonValue, LabelHtmlFor,
           LabelForm, LabelFocus, LabelAccessKey, FieldSetForm, LegendForm, LegendAccessKey,
           LegendAlign, LegendFocus, UListType, UListCompact, OListStart, OListCompact,
           OListType, DListCompact, DirectoryCompact, MenuCompact, LIType,
           LIValue, DivAlign, ParagraphAlign, HeadingAlign, BlockQuoteCite,
           QuoteCite, PreWidth, PreWrap, BRClear, BaseFontColor, BaseFontSize,
           BaseFontFace, FontColor, FontSize, FontFace, HRWidth, HRNoShade,
           HRAlign, HRSize, ModCite, ModDateTime, AnchorShape, AnchorRel,
           AnchorAccessKey, AnchorCoords, AnchorHref, AnchorProtocol, AnchorHost,
           AnchorCharset, AnchorHrefLang, AnchorHostname, AnchorType, AnchorFocus,
           AnchorPort, AnchorPathName, AnchorHash, AnchorSearch, AnchorName,
           AnchorRev, AnchorTabIndex, AnchorTarget, AnchorText, AnchorBlur, AnchorToString, 
           ImageName, ImageAlign, ImageHspace, ImageVspace, ImageUseMap, ImageAlt,
           ImageLowSrc, ImageWidth, ImageIsMap, ImageBorder, ImageComplete, ImageHeight,
           ImageLongDesc, ImageSrc, ImageX, ImageY, ObjectHspace, ObjectHeight, ObjectAlign,
           ObjectBorder, ObjectCode, ObjectType, ObjectVspace, ObjectArchive,
           ObjectDeclare, ObjectForm, ObjectCodeBase, ObjectCodeType, ObjectData,
           ObjectName, ObjectStandby, ObjectTabIndex, ObjectUseMap, ObjectWidth, ObjectContentDocument,
           ParamName, ParamType, ParamValueType, ParamValue, AppletArchive,
           AppletAlt, AppletCode, AppletWidth, AppletAlign, AppletCodeBase,
           AppletName, AppletHeight, AppletHspace, AppletObject, AppletVspace,
           MapAreas, MapName, AreaHash, AreaHref, AreaTarget, AreaPort, AreaShape,
           AreaCoords, AreaAlt, AreaAccessKey, AreaNoHref, AreaHost, AreaProtocol,
           AreaHostName, AreaPathName, AreaSearch, AreaTabIndex, ScriptEvent,
           ScriptType, ScriptHtmlFor, ScriptText, ScriptSrc, ScriptCharset,
           ScriptDefer, TableSummary, TableTBodies, TableTHead, TableCellPadding,
           TableDeleteCaption, TableCreateCaption, TableCaption, TableWidth,
           TableCreateTFoot, TableAlign, TableTFoot, TableDeleteRow,
           TableCellSpacing, TableRows, TableBgColor, TableBorder, TableFrame,
           TableRules, TableCreateTHead, TableDeleteTHead, TableDeleteTFoot,
           TableInsertRow, TableCaptionAlign, TableColCh, TableColChOff,
           TableColAlign, TableColSpan, TableColVAlign, TableColWidth,
           TableSectionCh, TableSectionDeleteRow, TableSectionChOff,
           TableSectionRows, TableSectionAlign, TableSectionVAlign,
           TableSectionInsertRow, TableRowSectionRowIndex, TableRowRowIndex,
           TableRowChOff, TableRowCells, TableRowVAlign, TableRowCh,
           TableRowAlign, TableRowBgColor, TableRowDeleteCell, TableRowInsertCell,
           TableCellColSpan, TableCellNoWrap, TableCellAbbr, TableCellHeight,
           TableCellWidth, TableCellCellIndex, TableCellChOff, TableCellBgColor,
           TableCellCh, TableCellVAlign, TableCellRowSpan, TableCellHeaders,
           TableCellAlign, TableCellAxis, TableCellScope, FrameSetCols,
           FrameSetRows, FrameSrc, FrameLocation, FrameFrameBorder, FrameScrolling,
           FrameMarginWidth, FrameLongDesc, FrameMarginHeight, FrameName, FrameContentDocument, FrameContentWindow, 
           FrameNoResize, IFrameLongDesc, IFrameDocument, IFrameAlign,
           IFrameFrameBorder, IFrameSrc, IFrameName, IFrameHeight,
           IFrameMarginHeight, IFrameMarginWidth, IFrameScrolling, IFrameWidth, IFrameContentDocument, IFrameContentWindow,
           MarqueeStart, MarqueeStop,
           GetContext,
           ElementInnerHTML, ElementTitle, ElementId, ElementDir, ElementLang,
           ElementClassName, ElementInnerText, ElementDocument, ElementChildren, ElementContentEditable,
           ElementIsContentEditable, ElementOuterHTML, ElementOuterText};
  private:
    static ValueImp *formIndexGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *formNameGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *selectIndexGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *framesetNameGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *frameWindowPropertyGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *runtimeObjectGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *runtimeObjectPropertyGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
  };

  DOM::HTMLElementImpl *toHTMLElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object
  DOM::HTMLTableCaptionElementImpl *toHTMLTableCaptionElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object for a HTMLTableCaptionElementImpl
  DOM::HTMLTableSectionElementImpl *toHTMLTableSectionElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object for a HTMLTableSectionElementImpl

  class HTMLCollection : public DOMObject {
  public:
    HTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c);
    ~HTMLCollection();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List&args);
    virtual bool implementsCall() const { return true; }
    virtual bool toBoolean(ExecState *) const { return true; }
    enum { Item, NamedItem, Tags };
    ValueImp *getNamedItems(ExecState *exec, const Identifier &propertyName) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::HTMLCollectionImpl *impl() const { return m_impl.get(); }
  protected:
    RefPtr<DOM::HTMLCollectionImpl> m_impl;
  private:
    static ValueImp *lengthGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *indexGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *nameGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
  };

  class HTMLSelectCollection : public HTMLCollection {
  public:
    HTMLSelectCollection(ExecState *exec, DOM::HTMLCollectionImpl *c, DOM::HTMLSelectElementImpl *e);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
  private:
    static ValueImp *selectedIndexGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);

    RefPtr<DOM::HTMLSelectElementImpl> m_element;
  };

  ////////////////////// Option Object ////////////////////////

  class OptionConstructorImp : public ObjectImp {
  public:
    OptionConstructorImp(ExecState *exec, DOM::DocumentImpl *d);
    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);
  private:
    RefPtr<DOM::DocumentImpl> m_doc;
  };

  ////////////////////// Image Object ////////////////////////

  class ImageConstructorImp : public ObjectImp {
  public:
    ImageConstructorImp(ExecState *exec, DOM::DocumentImpl *d);
    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);
  private:
    RefPtr<DOM::DocumentImpl> m_doc;
  };

  ////////////////////// Context2D Object ////////////////////////

  class Context2D : public DOMObject {
  friend class Context2DFunction;
  public:
    Context2D(DOM::HTMLElementImpl *e);
    ~Context2D();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual void mark();
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    enum { 
        StrokeStyle,
        FillStyle,
        LineWidth,
        LineCap,
        LineJoin,
        MiterLimit,
        ShadowOffsetX,
        ShadowOffsetY,
        ShadowBlur,
        ShadowColor,
        GlobalAlpha,
        GlobalCompositeOperation,
        Save, Restore,
        Scale, Rotate, Translate,
        BeginPath, ClosePath, 
        SetStrokeColor, SetFillColor, SetLineWidth, SetLineCap, SetLineJoin, SetMiterLimit, 
        Fill, Stroke, 
        MoveTo, LineTo, QuadraticCurveTo, BezierCurveTo, ArcTo, Arc, Rect, Clip,
        ClearRect, FillRect, StrokeRect,
        DrawImage, DrawImageFromRect,
        SetShadow, ClearShadow,
        SetAlpha, SetCompositeOperation,
        CreateLinearGradient,
        CreateRadialGradient,
        CreatePattern
    };

private:
    void save();
    void restore();
    
    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGContextRef drawingContext();
    CGAffineTransform _lastFillImagePatternCTM;
    CGAffineTransform _lastStrokeImagePatternCTM;
    bool _validFillImagePattern;
    bool _validStrokeImagePattern;
    void updateFillImagePattern();
    void updateStrokeImagePattern();
    
    void setShadow(ExecState *exec);

    RefPtr<DOM::HTMLElementImpl> _element;
    bool _needsFlushRasterCache;
    
    QPtrList<List> stateStack;
    
    ValueImp *_strokeStyle;
    ValueImp *_fillStyle;
    ValueImp *_lineWidth;
    ValueImp *_lineCap;
    ValueImp *_lineJoin;
    ValueImp *_miterLimit;
    ValueImp *_shadowOffsetX;
    ValueImp *_shadowOffsetY;
    ValueImp *_shadowBlur;
    ValueImp *_shadowColor;
    ValueImp *_globalAlpha;
    ValueImp *_globalComposite;
  };

    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGColorRef colorRefFromValue(ExecState *exec, ValueImp *value);

    QColor colorFromValue(ExecState *exec, ValueImp *value);

    struct ColorStop {
        float stop;
        float red;
        float green;
        float blue;
        float alpha;
        
        ColorStop(float s, float r, float g, float b, float a) : stop(s), red(r), green(g), blue(b), alpha(a) {};
    };

  class Gradient : public DOMObject {
  friend class Context2DFunction;
  public:
    Gradient(float x0, float y0, float x1, float y1);
    Gradient(float x0, float y0, float r0, float x1, float y1, float r1);
    ~Gradient();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    enum { 
        AddColorStop
    };
    
    enum {
        Radial, Linear
    };

    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGShadingRef getShading();
    
    void addColorStop (float s, float r, float g, float b, float alpha);
    const ColorStop *colorStops(int *count) const;
    
    int lastStop;
    int nextStop;
    
private:    
    void commonInit();
    
    int _gradientType;
    float _x0, _y0, _r0, _x1, _y1, _r1;

    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGShadingRef _shadingRef;
    
    int maxStops;
    int stopCount;
    ColorStop *stops;
    mutable int adjustedStopCount;
    mutable ColorStop *adjustedStops;
    mutable unsigned stopsNeedAdjusting:1;
    mutable unsigned regenerateShading:1;
  };

  class ImagePattern : public DOMObject {
  public:
    ImagePattern(DOM::HTMLImageElementImpl *i, int type);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    
    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGPatternRef createPattern(CGAffineTransform transform);
    
    QPixmap pixmap() { return _pixmap; }
    
    enum {
        Repeat, RepeatX, RepeatY, NoRepeat
    };
    
private:
    float _rw, _rh;
    QPixmap _pixmap;
    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGRect _bounds;
  };

  ValueImp *getHTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c);
  ValueImp *getSelectHTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c, DOM::HTMLSelectElementImpl *e);

} // namespace

#endif
