typedef union {
    CSSRuleImpl *rule;
    CSSSelector *selector;
    QPtrList<CSSSelector> *selectorList;
    bool ok;
    MediaListImpl *mediaList;
    CSSMediaRuleImpl *mediaRule;
    CSSRuleListImpl *ruleList;
    ParseString string;
    float val;
    int prop_id;
    int attribute;
    int element;
    CSSSelector::Relation relation;
    bool b;
    char tok;
    Value value;
    ValueList *valueList;
} YYSTYPE;
#define	S	257
#define	SGML_CD	258
#define	INCLUDES	259
#define	DASHMATCH	260
#define	BEGINSWITH	261
#define	ENDSWITH	262
#define	CONTAINS	263
#define	STRING	264
#define	IDENT	265
#define	HASH	266
#define	IMPORT_SYM	267
#define	PAGE_SYM	268
#define	MEDIA_SYM	269
#define	FONT_FACE_SYM	270
#define	CHARSET_SYM	271
#define	KONQ_RULE_SYM	272
#define	KONQ_DECLS_SYM	273
#define	KONQ_VALUE_SYM	274
#define	IMPORTANT_SYM	275
#define	QEMS	276
#define	EMS	277
#define	EXS	278
#define	PXS	279
#define	CMS	280
#define	MMS	281
#define	INS	282
#define	PTS	283
#define	PCS	284
#define	DEGS	285
#define	RADS	286
#define	GRADS	287
#define	MSECS	288
#define	SECS	289
#define	HERZ	290
#define	KHERZ	291
#define	DIMEN	292
#define	PERCENTAGE	293
#define	NUMBER	294
#define	URI	295
#define	FUNCTION	296
#define	UNICODERANGE	297

