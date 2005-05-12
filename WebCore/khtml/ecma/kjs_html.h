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

#if APPLE_CHANGES
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace DOM {
    class HTMLCollectionImpl;
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class HTMLSelectElementImpl;
    class HTMLTableCaptionElementImpl;
    class HTMLTableSectionElementImpl;
};

namespace KJS {

  class JSAbstractEventListener;

  class HTMLDocument : public DOMDocument {
  public:
    HTMLDocument(ExecState *exec, DOM::HTMLDocumentImpl *d);
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
    virtual bool hasProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Title, Referrer, Domain, URL, Body, Location, Cookie,
           Images, Applets, Embeds, Links, Forms, Anchors, Scripts, All, Clear, Open, Close,
           Write, WriteLn, GetElementsByName, CaptureEvents, ReleaseEvents,
           BgColor, FgColor, AlinkColor, LinkColor, VlinkColor, LastModified, Height, Width, Dir, DesignMode };
  };

  class HTMLElement : public DOMElement {
  public:
    HTMLElement(ExecState *exec, DOM::HTMLElementImpl *e);
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int);
    virtual bool hasProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual UString toString(ExecState *exec) const;
    virtual void pushEventHandlerScope(ExecState *exec, ScopeChain &scope) const;
    virtual Value call(ExecState *exec, Object &thisObj, const List&args);
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
           InputChecked, InputForm, InputAccessKey, InputAlign, InputAlt,
           InputName, InputSrc, InputUseMap, InputSelect, InputClick,
           TextAreaAccessKey, TextAreaName, TextAreaDefaultValue, TextAreaSelect,
           TextAreaCols, TextAreaDisabled, TextAreaForm, TextAreaType,
           TextAreaTabIndex, TextAreaReadOnly, TextAreaRows, TextAreaValue,
           TextAreaBlur, TextAreaFocus, ButtonBlur, ButtonFocus, ButtonForm, ButtonTabIndex, ButtonName,
           ButtonDisabled, ButtonAccessKey, ButtonType, ButtonValue, LabelHtmlFor,
           LabelForm, LabelAccessKey, FieldSetForm, LegendForm, LegendAccessKey,
           LegendAlign, UListType, UListCompact, OListStart, OListCompact,
           OListType, DListCompact, DirectoryCompact, MenuCompact, LIType,
           LIValue, DivAlign, ParagraphAlign, HeadingAlign, BlockQuoteCite,
           QuoteCite, PreWidth, BRClear, BaseFontColor, BaseFontSize,
           BaseFontFace, FontColor, FontSize, FontFace, HRWidth, HRNoShade,
           HRAlign, HRSize, ModCite, ModDateTime, AnchorShape, AnchorRel,
           AnchorAccessKey, AnchorCoords, AnchorHref, AnchorProtocol, AnchorHost,
           AnchorCharset, AnchorHrefLang, AnchorHostname, AnchorType, AnchorFocus,
           AnchorPort, AnchorPathName, AnchorHash, AnchorSearch, AnchorName,
           AnchorRev, AnchorTabIndex, AnchorTarget, AnchorText, AnchorBlur, AnchorToString, 
           ImageName, ImageAlign, ImageHspace, ImageVspace, ImageUseMap, ImageAlt,
           ImageLowSrc, ImageWidth, ImageIsMap, ImageBorder, ImageHeight,
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
  };

  DOM::HTMLElementImpl *toHTMLElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object
  DOM::HTMLTableCaptionElementImpl *toHTMLTableCaptionElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object for a HTMLTableCaptionElementImpl
  DOM::HTMLTableSectionElementImpl *toHTMLTableSectionElement(ValueImp *); // returns 0 if passed-in value is not a HTMLElement object for a HTMLTableSectionElementImpl

  class HTMLCollection : public DOMObject {
  public:
    HTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c);
    ~HTMLCollection();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    virtual Value call(ExecState *exec, Object &thisObj, const List&args);
    virtual Value tryCall(ExecState *exec, Object &thisObj, const List&args);
    virtual bool implementsCall() const { return true; }
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual bool hasProperty(ExecState *exec, const Identifier &p) const;
    enum { Item, NamedItem, Tags };
    Value getNamedItems(ExecState *exec, const Identifier &propertyName) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::HTMLCollectionImpl *impl() const { return m_impl.get(); }
  protected:
    khtml::SharedPtr<DOM::HTMLCollectionImpl> m_impl;
  };

  class HTMLSelectCollection : public HTMLCollection {
  public:
    HTMLSelectCollection(ExecState *exec, DOM::HTMLCollectionImpl *c, DOM::HTMLSelectElementImpl *e);
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
  private:
    khtml::SharedPtr<DOM::HTMLSelectElementImpl> m_element;
  };

  ////////////////////// Option Object ////////////////////////

  class OptionConstructorImp : public ObjectImp {
  public:
    OptionConstructorImp(ExecState *exec, DOM::DocumentImpl *d);
    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);
  private:
    khtml::SharedPtr<DOM::DocumentImpl> m_doc;
  };

  ////////////////////// Image Object ////////////////////////

  class ImageConstructorImp : public ObjectImp {
  public:
    ImageConstructorImp(ExecState *exec, DOM::DocumentImpl *d);
    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);
  private:
    khtml::SharedPtr<DOM::DocumentImpl> m_doc;
  };

  class Image : public DOMObject, public khtml::CachedObjectClient {
  public:
    Image(DOM::DocumentImpl *d, bool ws, int w, bool hs, int h);
    ~Image();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
    void notifyFinished(khtml::CachedObject *);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Src, Complete, OnLoad, Width, Height };
    
    khtml::CachedImage* image() { return img; }
    
  private:
    UString src;
    QGuardedPtr<DOM::DocumentImpl> doc;
    khtml::CachedImage* img;
    JSAbstractEventListener *onLoadListener;
    bool widthSet;
    bool heightSet;
    int width;
    int height;
  };

  ////////////////////// Context2D Object ////////////////////////

  class Context2D : public DOMObject {
  friend class Context2DFunction;
  public:
    Context2D(DOM::HTMLElementImpl *e);
    ~Context2D();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
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

    void setShadow(ExecState *exec);

    khtml::SharedPtr<DOM::HTMLElementImpl> _element;
    bool _needsFlushRasterCache;
    
    QPtrList<List> stateStack;
    
    Value _strokeStyle;
    Value _fillStyle;
    Value _lineWidth;
    Value _lineCap;
    Value _lineJoin;
    Value _miterLimit;
    Value _shadowOffsetX;
    Value _shadowOffsetY;
    Value _shadowBlur;
    Value _shadowColor;
    Value _globalAlpha;
    Value _globalComposite;
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
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
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
    ImagePattern(Image *i, int type);
    ~ImagePattern();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    
    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPainter.
    CGPatternRef getPattern() { return _patternRef; }
    
    QPixmap pixmap() { return _pixmap; }
    
    enum {
        Repeat, RepeatX, RepeatY, NoRepeat
    };
    
private:
    int _repetitionType;
    QPixmap _pixmap;

    // FIXME: Macintosh specific, and should be abstracted by KWQ in QPixmap.
    CGPatternRef _patternRef;
  };

  ValueImp *getHTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c);
  ValueImp *getSelectHTMLCollection(ExecState *exec, DOM::HTMLCollectionImpl *c, DOM::HTMLSelectElementImpl *e);

} // namespace

#endif
