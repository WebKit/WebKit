
extern int timeclock;


int yyerror;		/*  Yyerror and yycost are set by guards.	*/
int yycost;		/*  If yyerror is set to a nonzero value by a	*/
			/*  guard, the reduction with which the guard	*/
			/*  is associated is not performed, and the	*/
			/*  error recovery mechanism is invoked.	*/
			/*  Yycost indicates the cost of performing	*/
			/*  the reduction given the attributes of the	*/
			/*  symbols.					*/


/*  YYMAXDEPTH indicates the size of the parser's state and value	*/
/*  stacks.								*/

#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	500
#endif

/*  YYMAXRULES must be at least as large as the number of rules that	*/
/*  could be placed in the rule queue.  That number could be determined	*/
/*  from the grammar and the size of the stack, but, as yet, it is not.	*/

#ifndef	YYMAXRULES
#define	YYMAXRULES	100
#endif

#ifndef	YYMAXBACKUP
#define YYMAXBACKUP	100
#endif


short	yyss[YYMAXDEPTH];	/*  the state stack			*/
YYSTYPE	yyvs[YYMAXDEPTH];	/*  the semantic value stack		*/
YYLTYPE yyls[YYMAXDEPTH];	/*  the location stack			*/
short	yyrq[YYMAXRULES];	/*  the rule queue			*/
int	yychar;			/*  the lookahead symbol		*/

YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

YYSTYPE yytval;			/*  the semantic value for the state	*/
				/*  at the top of the state stack.	*/

YYSTYPE yyval;			/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

YYLTYPE yylloc;		/*  location data for the lookahead	*/
				/*  symbol				*/

YYLTYPE yytloc;		/*  location data for the state at the	*/
				/*  top of the state stack		*/


int	yynunlexed;
short	yyunchar[YYMAXBACKUP];
YYSTYPE	yyunval[YYMAXBACKUP];
YYLTYPE yyunloc[YYMAXBACKUP];

short *yygssp;			/*  a pointer to the top of the state	*/
				/*  stack; only set during error	*/
				/*  recovery.				*/

YYSTYPE *yygvsp;		/*  a pointer to the top of the value	*/
				/*  stack; only set during error	*/
				/*  recovery.				*/

YYLTYPE *yyglsp;		/*  a pointer to the top of the		*/
				/*  location stack; only set during	*/
				/*  error recovery.			*/


/*  Yyget is an interface between the parser and the lexical analyzer.	*/
/*  It is costly to provide such an interface, but it avoids requiring	*/
/*  the lexical analyzer to be able to back up the scan.		*/

yyget()
{
  if (yynunlexed > 0)
    {
      yynunlexed--;
      yychar = yyunchar[yynunlexed];
      yylval = yyunval[yynunlexed];
      yylloc = yyunloc[yynunlexed];
    }
  else if (yychar <= 0)
    yychar = 0;
  else
    {
      yychar = yylex();
      if (yychar < 0)
	yychar = 0;
      else yychar = YYTRANSLATE(yychar);
    }
}



yyunlex(chr, val, loc)
int chr;
YYSTYPE val;
YYLTYPE loc;
{
  yyunchar[yynunlexed] = chr;
  yyunval[yynunlexed] = val;
  yyunloc[yynunlexed] = loc;
  yynunlexed++;
}



yyrestore(first, last)
register short *first;
register short *last;
{
  register short *ssp;
  register short *rp;
  register int symbol;
  register int state;
  register int tvalsaved;

  ssp = yygssp;
  yyunlex(yychar, yylval, yylloc);

  tvalsaved = 0;
  while (first != last)
    {
      symbol = yystos[*ssp];
      if (symbol < YYNTBASE)
	{
	  yyunlex(symbol, yytval, yytloc);
	  tvalsaved = 1;
	  ssp--;
	}

      ssp--;

      if (first == yyrq)
	first = yyrq + YYMAXRULES;

      first--;

      for (rp = yyrhs + yyprhs[*first]; symbol = *rp; rp++)
	{
	  if (symbol < YYNTBASE)
	    state = yytable[yypact[*ssp] + symbol];
	  else
	    {
	      state = yypgoto[symbol - YYNTBASE] + *ssp;

	      if (state >= 0 && state <= YYLAST && yycheck[state] == *ssp)
		state = yytable[state];
	      else
		state = yydefgoto[symbol - YYNTBASE];
	    }

	  *++ssp = state;
	}
    }

  if ( ! tvalsaved && ssp > yyss)
    {
      yyunlex(yystos[*ssp], yytval, yytloc);
      ssp--;
    }

  yygssp = ssp;
}



int
yyparse()
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register short *yyrq0;
  register short *yyptr;
  register YYSTYPE *yyvsp;

  int yylen;
  YYLTYPE *yylsp;
  short *yyrq1;
  short *yyrq2;

  yystate = 0;
  yyssp = yyss - 1;
  yyvsp = yyvs - 1;
  yylsp = yyls - 1;
  yyrq0 = yyrq;
  yyrq1 = yyrq0;
  yyrq2 = yyrq0;

  yychar = yylex();
  if (yychar < 0)
    yychar = 0;
  else yychar = YYTRANSLATE(yychar);

yynewstate:

  if (yyssp >= yyss + YYMAXDEPTH - 1)
    {
      yyabort("Parser Stack Overflow");
      YYABORT;
    }

  *++yyssp = yystate;

yyresume:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  yyn += yychar;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar)
    goto yydefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  yystate = yyn;

  yyptr = yyrq2;
  while (yyptr != yyrq1)
    {
      yyn = *yyptr++;
      yylen = yyr2[yyn];
      yyvsp -= yylen;
      yylsp -= yylen;

      yyguard(yyn, yyvsp, yylsp);
      if (yyerror)
	goto yysemerr;

      yyaction(yyn, yyvsp, yylsp);
      *++yyvsp = yyval;

      yylsp++;
      if (yylen == 0)
	{
	  yylsp->timestamp = timeclock;
	  yylsp->first_line = yytloc.first_line;
	  yylsp->first_column = yytloc.first_column;
	  yylsp->last_line = (yylsp-1)->last_line;
	  yylsp->last_column = (yylsp-1)->last_column;
	  yylsp->text = 0;
	}
      else
	{
	  yylsp->last_line = (yylsp+yylen-1)->last_line;
	  yylsp->last_column = (yylsp+yylen-1)->last_column;
	}
	  
      if (yyptr == yyrq + YYMAXRULES)
        yyptr = yyrq;
    }

  if (yystate == YYFINAL)
    YYACCEPT;

  yyrq2 = yyptr;
  yyrq1 = yyrq0;

  *++yyvsp = yytval;
  *++yylsp = yytloc;
  yytval = yylval;
  yytloc = yylloc;
  yyget();

  goto yynewstate;

yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

yyreduce:

  *yyrq0++ = yyn;

  if (yyrq0 == yyrq + YYMAXRULES)
    yyrq0 = yyrq;

  if (yyrq0 == yyrq2)
    {
      yyabort("Parser Rule Queue Overflow");
      YYABORT;
    }

  yyssp -= yyr2[yyn];
  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yysemerr:
  *--yyptr = yyn;
  yyrq2 = yyptr;
  yyvsp += yyr2[yyn];

yyerrlab:

  yygssp = yyssp;
  yygvsp = yyvsp;
  yyglsp = yylsp;
  yyrestore(yyrq0, yyrq2);
  yyrecover();
  yystate = *yygssp;
  yyssp = yygssp;
  yyvsp = yygvsp;
  yyrq0 = yyrq;
  yyrq1 = yyrq0;
  yyrq2 = yyrq0;
  goto yyresume;
}

$
