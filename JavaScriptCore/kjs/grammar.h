typedef union {
  int                 ival;
  double              dval;
  UString             *ustr;
  Identifier          *ident;
  Node                *node;
  StatementNode       *stat;
  ParameterNode       *param;
  FunctionBodyNode    *body;
  FuncDeclNode        *func;
  ProgramNode         *prog;
  AssignExprNode      *init;
  SourceElementsNode  *srcs;
  StatListNode        *slist;
  ArgumentsNode       *args;
  ArgumentListNode    *alist;
  VarDeclNode         *decl;
  VarDeclListNode     *vlist;
  CaseBlockNode       *cblk;
  ClauseListNode      *clist;
  CaseClauseNode      *ccl;
  ElementNode         *elm;
  Operator            op;
  PropertyValueNode   *plist;
  PropertyNode        *pnode;
  CatchNode           *cnode;
  FinallyNode         *fnode;
} YYSTYPE;

#ifndef YYLTYPE
typedef
  struct yyltype
    {
      int timestamp;
      int first_line;
      int first_column;
      int last_line;
      int last_column;
      char *text;
   }
  yyltype;

#define YYLTYPE yyltype
#endif

#define	NULLTOKEN	257
#define	TRUETOKEN	258
#define	FALSETOKEN	259
#define	STRING	260
#define	NUMBER	261
#define	BREAK	262
#define	CASE	263
#define	DEFAULT	264
#define	FOR	265
#define	NEW	266
#define	VAR	267
#define	CONTINUE	268
#define	FUNCTION	269
#define	RETURN	270
#define	VOID	271
#define	DELETE	272
#define	IF	273
#define	THIS	274
#define	DO	275
#define	WHILE	276
#define	ELSE	277
#define	IN	278
#define	INSTANCEOF	279
#define	TYPEOF	280
#define	SWITCH	281
#define	WITH	282
#define	RESERVED	283
#define	THROW	284
#define	TRY	285
#define	CATCH	286
#define	FINALLY	287
#define	EQEQ	288
#define	NE	289
#define	STREQ	290
#define	STRNEQ	291
#define	LE	292
#define	GE	293
#define	OR	294
#define	AND	295
#define	PLUSPLUS	296
#define	MINUSMINUS	297
#define	LSHIFT	298
#define	RSHIFT	299
#define	URSHIFT	300
#define	PLUSEQUAL	301
#define	MINUSEQUAL	302
#define	MULTEQUAL	303
#define	DIVEQUAL	304
#define	LSHIFTEQUAL	305
#define	RSHIFTEQUAL	306
#define	URSHIFTEQUAL	307
#define	ANDEQUAL	308
#define	MODEQUAL	309
#define	XOREQUAL	310
#define	OREQUAL	311
#define	IDENT	312
#define	AUTOPLUSPLUS	313
#define	AUTOMINUSMINUS	314


extern YYSTYPE kjsyylval;
