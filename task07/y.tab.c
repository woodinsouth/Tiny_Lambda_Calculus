/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20170709

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0
#define YYPREFIX "yy"

#define YYPURE 0

#line 1 "control.y"

/* control flow translation using backpatching 
	(C) hanfei.wang@gmail.com  
	2017.11.27

  last modification: support the elimination of redundant goto.
  see the detail of the semantic rules and translation schema
  in the text file readme.pdf       			
                  
  supports:
  
  1/ if then else
  2/ switch case
  3/ while do
  4/ repeat until
  5/ for i := e to e do 
  6/ break and continue in loop structure.
  7/ function call in expression
  8/ boolean expression translates in control flow 
  9/ array element

*/


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef GCC
#else
#include <alloc.h>
#endif


char last_mark [] = "last";

extern char *yytext;         /* Lexeme, In yylex()           */
char *new_label( void );
char *new_name  ( void    );  
void free_name  ( char *s );

typedef struct code {
  char * code;     
  char * label; /* for goto label, used also 
                   for backpatching list pointer */
  struct code * next;
} CODE;
 
typedef struct att {
  CODE * true_list;      /* For E.truelist and S.nextlist */
  CODE * false_list;
  CODE * code;      
  CODE * break_list;     /* add for the break statement */ 
  CODE * continue_list;  /* add for continue statement */
} ATT;


void print_code(CODE * code)
{        
  int last_label = 0;
  
  printf("##code _listing:\n");
  
  while (code != NULL) {
    if (code->code != NULL && code->label!=NULL){
      if (!last_label) printf("\t");
      last_label = 0;
      printf("%s %s \n", code->code, code->label);
    }
    else if (code->code != NULL){
      if (!last_label) printf("\t");
      last_label = 0;
      printf("%s\n", code->code);
    }
    else if (code->label!= NULL){
      last_label = 1;
      printf("%s:\t", code->label);
    }
    code = code ->next;
  }
  printf("\n##end_listing\n");
     
}

CODE * make_code(char *code, char *label)
{
  CODE * tmp = (CODE *) malloc( sizeof(CODE)) ;
  if (code != NULL) {
    char * s = (char *) malloc( strlen(code)+1);
    strcpy(s, code);
    tmp -> code = s;
  } else 
    tmp -> code = NULL;
  
  tmp -> label = label;
  tmp -> next = NULL;
  return tmp;
}

ATT make_att(CODE * tl, CODE * fl, CODE * code)
{
  ATT * att = (ATT * ) malloc(sizeof(ATT));
  att -> true_list = tl;
  att -> false_list = fl;
  att -> break_list = NULL;
  att -> continue_list = NULL;
  att-> code = code;
  return * att;
}

CODE *join_code(CODE * code1, CODE * code2)
{
  CODE * forward  = code1;
  if (forward == NULL) return code2;
  while( forward->next != NULL) forward = forward->next;
  forward->next = code2;
  return code1;
  
}

CODE * get_first(CODE * code)
{
  if (code == NULL) {
    printf("get the beginning of code list error!\n");
    exit(1);
  }
  
  if (code->code  ==  NULL && code->label != NULL) 
    return code;
  {
    char * new_l = new_label();
    CODE * new_code = make_code(NULL, new_l);
    new_code -> next = code;
    
    return new_code;
  }
}


CODE *merge(CODE *code1, CODE *code2)
{ 
  /* must merge as the order code1 first, code2 last */ 
  CODE * forward = code1;
  
  if (forward == NULL) return code2;
  if (code2 == NULL) return code1;
  while(forward->label != NULL) forward = (CODE *) forward ->label;
  forward->label = (char * ) code2;
  return code1;
}

/* cut the tail of code1 list */ 
CODE *cut_last(CODE * code1, CODE * last)
{ 
  CODE *forward = code1;
  
  if (forward == NULL) return NULL;
  if (last == NULL) return code1;
  if (code1 == last) return NULL;
  while( forward -> label != (char *) last) 
    forward = (CODE *) forward ->label;
  forward->label = NULL;
  return code1;
}

CODE * get_tail(CODE * code)
{
  CODE * forward = code;

  if (forward == NULL) return NULL;
  
  while( forward->label != NULL) forward = (CODE *) forward ->label;

  return forward;
}

void back_patching(CODE *code, char * label, CODE *last)
{
  /* if last is null then backpatching all list.
     if last is the tail then backpatching all list except the tail */

  CODE * tmp;
  if (code == last || code == NULL) return;
   
  while (code != last) {
    tmp = (CODE *) code ->label;
    code -> label = label;
    code = tmp;
  } 
 }

int is_boolean_constant(CODE *list)
{
   return list -> code[0];
}
 
ATT assign(ATT id, ATT e)
{
  char s[20];
  CODE * code;
  
  sprintf(s, "%s := %s", (char *) id.true_list, (char *) e.true_list);
  code = make_code (s, NULL);
  id.code = join_code(e.code, code);
  id.true_list = NULL;
  id.false_list = NULL;
  
  return id;
}

char b_true[] = "true";
char b_false[] = "false";

int is_boolean_true(ATT a)
{
  if (a.code -> code == 0) return 0;

  return strncmp("true", a.code->code,4) == 0;
}

int is_boolean_false(ATT a)
{
  if (a.code -> code == 0) return 0;
  return strncmp("false", a.code->code,5) == 0;
}

#define FILL_TRUE 1
#define FILL_FALSE 0

/* combine 2 relation expression B1 relop B2 with:
   1/ if the mode is FILL_TRUE, then backpacting the B1.truelist
      with the begin label of B2.code. and merge two falselist.
   2/ if the mode is FILL_FALSE, then backpatching the B1.falselist
      with the begin label of B2.code, and merge two truelist   */

ATT combine(ATT att1, ATT att2, int mode)
{
  CODE * truelist1 = att1.true_list, * falselist1 = att1.false_list;
  CODE * truelist2 = att2.true_list, * falselist2 = att2.false_list;
  CODE * truetail = get_tail (att1.true_list);
  CODE * falsetail = get_tail (att1.false_list);
  CODE * code = att2.code;
  CODE * plist, /* list to backpatching */
    * mergelist1, *mergelist2, /* list to merge */
    * ptail, /* tail of list ot backpatching */
    * unmergelist; /* B2's unmerged list */
  
  if (is_boolean_true(att1)) {
    if (mode == FILL_TRUE) return att2;
    else return att1;
  }

  if (is_boolean_false(att1)) {
    if (mode == FILL_FALSE) return att2;
    else return att1;
  }

  if (is_boolean_true(att2)) {
    if (mode == FILL_TRUE) return att1;
    else return att2;
  }

  if (is_boolean_false(att2)) {
    if (mode == FILL_FALSE) return att1;
    else return att2;
  }

  if ((truelist1 != truetail && mode == FILL_TRUE) ||
    (falselist1 != falsetail && mode == FILL_FALSE)) 
    code = get_first(att2.code);
  else code = att2.code;
	 /* only truelist or falselist has multiple entries,
	    we need generate le label of B2.code */

  if (mode == FILL_TRUE) {
    plist = truelist1;
    ptail = truetail;
    mergelist1 = falselist1;
    mergelist2 = falselist2;
    unmergelist = truelist1;
  } else {
    plist = falselist1;
    ptail = falsetail;
    mergelist1 = truelist1;
    mergelist2 = truelist2;
    unmergelist = falselist1;
  }
  {
    char * newcode = (char *) malloc (strlen (truetail->code) + 20);
    back_patching(plist, code->label, ptail);
    if ((mode && ptail -> code [0] == '1') || 
	(!mode && ptail -> code [0] == '0')  ) 
      sprintf(newcode, "ifnot %s goto ", ptail-> code + 1);
    else
      sprintf(newcode, "if %s goto ", ptail-> code +1);
    ptail -> code = newcode;
  }
  
  mergelist1 = merge(mergelist1, mergelist2);
  att1.code = join_code(att1.code, code);

  if (mode == FILL_TRUE) {
    att1.true_list = truelist2;
    att1.false_list = mergelist1;
  } else {
    att1.false_list = falselist2;
    att1.true_list = mergelist1;
  }
  return att1;
}


ATT translate_repeat(ATT body, ATT cond)
{
  /* TODO */
}

ATT translate_while (ATT cond, ATT body)
{
  /* TODO */
}

ATT translate_not(ATT a)
{

  CODE * tmp = a.true_list;
  CODE * truetail = get_tail (a.true_list);
  CODE * falsetail = get_tail (a.false_list);
  
  if (is_boolean_true(a)){
    a.code -> code = b_false;
    return a;
  }
  
  if (is_boolean_false(a)) {
    a.code -> code = b_true;
    return a;
  }

  if (truetail == falsetail) {
    if (truetail-> code [0] == '1')
      truetail-> code [0] = '0';
    else truetail-> code [0] = '1';
  }
  a.true_list = a.false_list;
  a.false_list = tmp;
    
  return a;
}

/* for if cond then break, just change the cond.true_list to 
   break_list. and false_list to true_list */

ATT merge_break_continue(ATT cond, ATT s)
{
  CODE * truetail = get_tail(cond.true_list);
  char * newcode = (char *) malloc (strlen (truetail->code) + 20);
  if (truetail -> code [0] == '1')  
    sprintf(newcode, "if %s goto ", truetail-> code + 1);
  else
    sprintf(newcode, "ifnot %s goto ", truetail-> code +1);
  truetail -> code = newcode;
  if (s.break_list != NULL) {
    cond.break_list = cond.true_list;
  } else
    cond.continue_list = cond.true_list;
  cond.true_list = cut_last(cond.false_list, truetail) ;
  cond.false_list = NULL;
  return cond;
}

ATT merge_break_continue_reverse(ATT cond, ATT s)
{
  CODE * falsetail = get_tail(cond.false_list);
  char * newcode = (char *) malloc (strlen (falsetail->code) + 20);
  if (falsetail -> code [0] == '1')  
    sprintf(newcode, "ifnot %s goto ", falsetail-> code + 1);
  else
    sprintf(newcode, "if %s goto ", falsetail-> code +1);
  falsetail -> code = newcode;
  if (s.break_list != NULL) {
    cond.break_list = cond.false_list;
  } else
    cond.continue_list = cond.false_list;
  cond.true_list = cut_last(cond.true_list, falsetail) ;
  cond.false_list = NULL;
  return cond;
}

int is_break(ATT s)
{
	if (s.break_list == NULL)
		return 0;
	if (s.break_list == s.code)
		return 1;
	else
		return 0;
}

int is_continue(ATT s)
{
	if (s.continue_list == NULL)
		return 0;
	if (s.continue_list == s.code)
		return 1;
	else
		return 0;
}

ATT translate_if_then (ATT cond, ATT s)
{

  CODE * tmp = cond.false_list;
  while(tmp != NULL)
  {
    tmp->label = (char *)tmp->next;
    tmp = tmp->next;
  }
  tmp = cond.code;
  while(tmp != NULL)
  {
    printf("%s\n", tmp->label);
    printf("%s\n", tmp->code);
    tmp = tmp->next;
  }
  ATT att = combine(cond, s, FILL_TRUE);
  att.true_list = cond.false_list;
  return att;
}

ATT translate_if_then_else (ATT cond, ATT s1, ATT s2)
{
  CODE * tmp = cond.false_list;
  while(tmp != NULL)
  {
    if(tmp->code != NULL) tmp->label = (char *)tmp->next;
    tmp = tmp->next;
  }

  ATT att = combine(cond, s2, FILL_FALSE);
  att.false_list = cond.true_list;

  CODE * goto_s = make_code("goto", NULL);
  att.code = join_code(att.code, goto_s);

  CODE * label_s = make_code(NULL, new_label());
  att.code = join_code(att.code, label_s);
  back_patching(att.false_list, label_s->label, NULL);
  att.code = join_code(att.code, s1.code);
  att.true_list = goto_s;
  att.false_list = NULL;
  return att;
}

#define YYSTYPE   ATT

#define YYMAXDEPTH   64
#define YYMAXERR     10
#define YYVERBOSE
#line 480 "y.tab.c"

#if ! defined(YYSTYPE) && ! defined(YYSTYPE_IS_DECLARED)
/* Default: YYSTYPE is the semantic value type. */
typedef int YYSTYPE;
# define YYSTYPE_IS_DECLARED 1
#endif

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define IF 257
#define THEN 258
#define ELSE 259
#define WHILE 260
#define DO 261
#define SBEGIN 262
#define END 263
#define ID 264
#define ASSIGN 265
#define BREAK 266
#define CONTINUE 267
#define AND 268
#define OR 269
#define NOT 270
#define TRUE 271
#define FALSE 272
#define RELOP 273
#define FOR 274
#define TO 275
#define CASE 276
#define SWITCH 277
#define DEFAULT 278
#define REPEAT 279
#define UNTIL 280
#define UMINUS 281
#define YYERRCODE 256
typedef short YYINT;
static const YYINT yylhs[] = {                           -1,
    0,    1,    1,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    6,    6,    6,    6,    5,    5,
    4,    4,    4,    4,    4,    4,    4,    7,    7,    3,
    3,    3,    3,    3,    3,    3,
};
static const YYINT yylen[] = {                            2,
    2,    1,    3,    1,    1,    4,    6,    4,    4,    8,
    4,    3,    3,    5,    0,    4,    5,    5,    1,    4,
    3,    3,    3,    2,    3,    1,    4,    1,    3,    3,
    3,    2,    3,    3,    1,    1,
};
static const YYINT yydefred[] = {                         0,
    0,    0,    0,    0,    4,    5,    0,    0,    0,    0,
    0,    2,    0,    0,    0,   35,   36,    0,    0,    0,
    0,   26,    0,    0,    0,    0,    0,    0,    0,    1,
    0,    0,    0,   32,   24,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   12,    0,    0,    0,    0,
    0,   15,    0,    3,    0,    0,   33,   25,    0,   31,
    0,    0,    0,    0,   22,    9,   11,   20,    0,    0,
    0,    0,   27,    0,    0,    0,   14,    0,    7,    0,
    0,    0,    0,    0,    0,   10,    0,    0,
};
static const YYINT yydgoto[] = {                         10,
   11,   12,   20,   21,   22,   71,   49,
};
static const YYINT yysindex[] = {                       -93,
   37,   37,  -93,  -83,    0,    0, -245,  -33,  -93,    0,
   27,    0, -239,   34,   37,    0,    0,  -33,   37, -142,
  -28,    0, -132,  -53,  -33, -218,  -33,   21,  -50,    0,
  -93,  -33,  -33,    0,    0,   16,   -8,  -93,   37,   37,
  -33,  -33,  -33,  -33,  -93,    0, -109,   89,  -14,  -33,
  118,    0,   37,    0,   89,   76,    0,    0, -205,    0,
 -207,   89,   29,   29,    0,    0,    0,    0,  -33,  -32,
 -180, -159,    0,  -93,   89,  -33,    0, -223,    0,    8,
   31,   33,  -93,  -93,  -93,    0,   52,   52,
};
static const YYINT yyrindex[] = {                         0,
    0,    0,    0, -172,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  -41,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  -13,    0,    0,
    0,    0,    0,    0,  -30,    0,    0,    0,   10,    0,
   35,   19,  -21,   -1,    0,    0,    0,    0,    0,    0,
    0,  -10,    0,    0,   26,    0,    0,    0,    0,    0,
    0,    0,    0, -176,    0,    0, -168, -148,
};
static const YYINT yygindex[] = {                         0,
   18,    1,   99,   80,   59,    0,  102,
};
#define YYTABLESIZE 315
static const YYINT yytable[] = {                         19,
   19,   19,   19,   19,   19,   47,   27,   25,   31,   44,
   42,   18,   43,   44,   42,   13,   43,   19,   26,   21,
   24,   21,   21,   21,   21,   32,   29,   28,   13,   69,
   28,   54,   58,   44,   42,    8,   43,   21,   59,   23,
   81,   23,   23,   23,   23,   66,   50,   54,    8,   44,
   42,   19,   43,   74,   82,    6,   57,   23,   13,   34,
   39,   13,   44,   42,   34,   43,   29,   13,    6,   29,
   44,   21,   30,   33,   79,   30,   19,   34,   68,   28,
   30,   18,   77,   86,    0,   31,   16,   28,   84,   13,
   85,   23,   19,   30,   17,   78,   13,   35,   37,   16,
   23,   87,   88,   13,   48,   13,   51,   17,   39,   40,
   31,   55,   48,   34,   18,   38,   73,   36,   29,   69,
   62,   63,   64,   65,   25,   39,   40,   18,   45,   70,
   44,   42,   13,   43,   56,   39,   40,   60,   61,    0,
    0,   13,   13,   13,    0,    0,    0,    1,   75,    0,
    2,   72,    3,   67,    4,   80,    5,    6,   58,   44,
   42,    0,   43,    1,    7,    0,    2,    8,    3,    9,
    4,    0,    5,    6,    0,    0,    0,    0,    0,    0,
    7,    0,    0,    8,    0,    9,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   46,
    0,    0,    0,    0,    0,    0,   19,   19,    0,   19,
   19,   19,    0,    0,    0,    0,   19,   19,   13,   53,
   14,   19,   13,   19,   19,    0,   21,   21,   19,   21,
   21,   21,   76,    0,   41,   13,   21,   21,    8,   13,
    0,   21,    8,   21,   21,    0,   23,   23,   21,   23,
   23,   23,    0,    0,   41,    8,   23,   23,   83,    8,
    0,   23,    6,   23,   23,    0,   34,   34,   23,   34,
    0,   34,   52,   39,   40,    6,   34,   34,    0,    6,
    0,    0,   30,   30,   34,   30,    0,   30,   34,    0,
   14,    0,    0,   30,    0,    0,   15,   16,   17,    0,
   30,    0,    0,    0,   30,
};
static const YYINT yycheck[] = {                         41,
   42,   43,   44,   45,   46,   59,   40,   91,   59,   42,
   43,   45,   45,   42,   43,   46,   45,   59,  264,   41,
    3,   43,   44,   45,   46,  265,    9,   41,   59,   44,
   44,   31,   41,   42,   43,   46,   45,   59,   38,   41,
  264,   43,   44,   45,   46,   45,  265,   47,   59,   42,
   43,   93,   45,  259,  278,   46,   41,   59,    0,   41,
  268,    3,   42,   43,   46,   45,   41,    9,   59,   44,
   42,   93,   46,   40,   74,   41,   40,   59,   93,   93,
   46,   45,  263,   83,   -1,   59,  263,    8,   58,   31,
   58,   93,  265,   59,  263,  276,   38,   18,   19,  276,
    2,   84,   85,   45,   25,   47,   27,  276,  268,  269,
   59,   32,   33,   15,  263,  258,   41,   19,   93,   44,
   41,   42,   43,   44,   91,  268,  269,  276,  261,   50,
   42,   43,   74,   45,   33,  268,  269,   39,   40,   -1,
   -1,   83,   84,   85,   -1,   -1,   -1,  257,   69,   -1,
  260,   53,  262,  263,  264,   76,  266,  267,   41,   42,
   43,   -1,   45,  257,  274,   -1,  260,  277,  262,  279,
  264,   -1,  266,  267,   -1,   -1,   -1,   -1,   -1,   -1,
  274,   -1,   -1,  277,   -1,  279,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  263,
   -1,   -1,   -1,   -1,   -1,   -1,  258,  259,   -1,  261,
  262,  263,   -1,   -1,   -1,   -1,  268,  269,  259,  280,
  264,  273,  263,  275,  276,   -1,  258,  259,  280,  261,
  262,  263,  275,   -1,  273,  276,  268,  269,  259,  280,
   -1,  273,  263,  275,  276,   -1,  258,  259,  280,  261,
  262,  263,   -1,   -1,  273,  276,  268,  269,  261,  280,
   -1,  273,  263,  275,  276,   -1,  258,  259,  280,  261,
   -1,  263,  262,  268,  269,  276,  268,  269,   -1,  280,
   -1,   -1,  258,  259,  276,  261,   -1,  263,  280,   -1,
  264,   -1,   -1,  269,   -1,   -1,  270,  271,  272,   -1,
  276,   -1,   -1,   -1,  280,
};
#define YYFINAL 10
#ifndef YYDEBUG
#define YYDEBUG 1
#endif
#define YYMAXTOKEN 281
#define YYUNDFTOKEN 291
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const yyname[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'","'*'","'+'","','","'-'","'.'",0,0,0,0,0,0,0,0,0,0,0,
"':'","';'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'['",
0,"']'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,"IF","THEN","ELSE","WHILE","DO","SBEGIN","END","ID","ASSIGN",
"BREAK","CONTINUE","AND","OR","NOT","TRUE","FALSE","RELOP","FOR","TO","CASE",
"SWITCH","DEFAULT","REPEAT","UNTIL","UMINUS",0,0,0,0,0,0,0,0,0,"illegal-symbol",
};
static const char *const yyrule[] = {
"$accept : p",
"p : s_list '.'",
"s_list : s",
"s_list : s_list ';' s",
"s : BREAK",
"s : CONTINUE",
"s : IF b_e THEN s",
"s : IF b_e THEN s ELSE s",
"s : REPEAT s_list UNTIL b_e",
"s : WHILE b_e DO s",
"s : FOR ID ASSIGN e TO e DO s",
"s : SBEGIN s_list ';' END",
"s : SBEGIN s_list END",
"s : l ASSIGN e",
"s : SWITCH e SBEGIN case_list END",
"case_list :",
"case_list : case_list CASE ID ':'",
"case_list : case_list CASE ID ':' s_list",
"case_list : case_list CASE DEFAULT ':' s_list",
"l : ID",
"l : ID '[' e_list ']'",
"e : e '+' e",
"e : e '*' e",
"e : e '-' e",
"e : '-' e",
"e : '(' e ')'",
"e : l",
"e : ID '(' e_list ')'",
"e_list : e",
"e_list : e_list ',' e",
"b_e : b_e OR b_e",
"b_e : b_e AND b_e",
"b_e : NOT b_e",
"b_e : '(' b_e ')'",
"b_e : e RELOP e",
"b_e : TRUE",
"b_e : FALSE",

};
#endif

int      yydebug;
int      yynerrs;

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 1188 "control.y"

/*----------------------------------------------------------------------*/
#ifdef __TURBOC__
#pragma argsused
#endif
/*----------------------------------------------------------------------*/
char  *Names[] = { "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7","t8","t9",
           "t10", "t11", "t12", "t13", "t14", "t15", "t16", "t17","t18","t19",
           "t20", "t21", "t22", "t23", "t24", "t25", "t26", "t27","t28","t29", 
           "t30", "t31", "t32", "t33", "t34", "t35", "t36", "t37","t38","t29"  };
char  **Namep  = Names;

char    *new_name()
{
  /* Return a temporary-variable name by popping one off the name stack.  */

  if( Namep >= &Names[ sizeof(Names)/sizeof(*Names) ] )
    {
      yyerror("Expression too complex\n");
      exit( 1 );
    }

  return( *Namep++ );
}

char  *LNames[] = { "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7","l8","l9" ,
            "l10", "l11", "l12", "l13", "l14", "l15", "l16", "l17","l18","l19"};
char  **LNamep  = LNames;

char    *new_label()
{
  /* Return a temporary-variable name by popping one off the name stack.  */

  if( LNamep >= &LNames[ sizeof(LNames)/sizeof(*LNames) ] )
    {
      yyerror("Expression too complex\n");
      exit( 1 );
    }

  return( *LNamep++ );
}


void free_name(s)
     char    *s;
{           /* Free up a previously allocated name */
  *--Namep = s;
}

int yyerror(char *s)
{
  printf("%s\n", s);
}


int main( argc, argv )
     int  argc;
     char **argv;
{

  yyparse();
  return 0;
}
#line 826 "y.tab.c"

#if YYDEBUG
#include <stdio.h>	/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yym = 0;
    yyn = 0;
    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        yychar = YYLEX;
        if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if (((yyn = yysindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if (((yyn = yyrindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag != 0) goto yyinrecovery;

    YYERROR_CALL("syntax error");

    goto yyerrlab; /* redundant goto avoids 'unused label' warning */
yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if (((yyn = yysindex[*yystack.s_mark]) != 0) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym > 0)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);

    switch (yyn)
    {
case 1:
#line 474 "control.y"
	{
  if(yystack.l_mark[-1].true_list != NULL) {
    CODE * label_s = make_code(NULL, new_label());
    yystack.l_mark[-1].code = join_code(yystack.l_mark[-1].code, label_s);
    back_patching(yystack.l_mark[-1].true_list, label_s->label, NULL);
  }
       
  if (yystack.l_mark[-1].break_list != NULL)
    printf("Error: break statement misplaced!\n");
  
  if (yystack.l_mark[-1].continue_list != NULL)
    printf("Error: continue statement misplaced!\n");
  
  print_code(yystack.l_mark[-1].code);
}
break;
case 3:
#line 492 "control.y"
	{
    CODE * code = yystack.l_mark[0].code;
    if (yystack.l_mark[-2].true_list != NULL ) {
      code = get_first (yystack.l_mark[0].code);
      back_patching(yystack.l_mark[-2].true_list, code->label, NULL);
    }  
    
    yystack.l_mark[-2].true_list = yystack.l_mark[0].true_list;   /* not break */
    
    yystack.l_mark[-2].break_list = merge(yystack.l_mark[-2].break_list, yystack.l_mark[0].break_list);        
    yystack.l_mark[-2].continue_list = merge(yystack.l_mark[-2].continue_list, yystack.l_mark[0].continue_list);        
    
    yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, code);
    
    yyval = yystack.l_mark[-2];
}
break;
case 4:
#line 510 "control.y"
	{
    CODE * goto_s = make_code("goto", NULL);
    ATT att = make_att( NULL, NULL,goto_s);
    att.break_list = goto_s;
    yyval = att;
    
    /*  while a > 0 do 
    switch a 
    begin 
    case 1: a:= 100 
    case 2: case 3: 
    begin a:=2; break end
                case 3: 
        begin a:=3; continue end 
                end.
        
        will translate the following  TAC:
                
	##code _listing:
	l3:	ifnot (a > 0) goto  l4 
		if (a <> 1) goto  l0 
		a := 100
		goto l3 
	l0:	if (a = 2) goto  l1 
		if (a <> 3) goto  l2 
	l1:	a := 2
		goto l3 
		goto l3 
	l2:	if (a <> 3) goto  l3 
		a := 3
		goto l3 
		goto l3 
		goto l3 
	l4:	

    */
}
break;
case 5:
#line 548 "control.y"
	{
  CODE * goto_s = make_code("goto", NULL);
  ATT att = make_att( NULL, NULL,goto_s);
  att.continue_list = goto_s;
  yyval = att;
}
break;
case 6:
#line 555 "control.y"
	{
  yyval = translate_if_then(yystack.l_mark[-2], yystack.l_mark[0]);
}
break;
case 7:
#line 559 "control.y"
	{
  yyval = translate_if_then_else(yystack.l_mark[-4], yystack.l_mark[-2], yystack.l_mark[0]);
}
break;
case 8:
#line 563 "control.y"
	{
  yyval = translate_repeat(yystack.l_mark[-2], yystack.l_mark[0]);
 }
break;
case 9:
#line 583 "control.y"
	{
  yyval = translate_while(yystack.l_mark[-2], yystack.l_mark[0]);
}
break;
case 10:
#line 587 "control.y"
	{
  CODE * goto_s = make_code ("goto", NULL);
  char s[40];
  CODE * if_s,  *increment, *tmp;
  
  sprintf(s, "if (%s > %s) goto ", (char *) yystack.l_mark[-6].true_list, 
	  (char *) yystack.l_mark[-2].true_list);
  if_s = make_code (s, NULL);
  
  sprintf(s, "%s := %s + 1", (char *) yystack.l_mark[-6].true_list, (char *) yystack.l_mark[-6].true_list);
  increment = make_code (s, NULL);
  
  increment -> next = goto_s;
  
  yystack.l_mark[-2].code = join_code (yystack.l_mark[-2].code, if_s);
  yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, yystack.l_mark[0].code);
  yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, increment);
  
  tmp = get_first(yystack.l_mark[-2].code);
  
  back_patching(yystack.l_mark[0].continue_list, tmp->label, NULL);
   /* backpatching continue list */
  
  yystack.l_mark[-6] = assign(yystack.l_mark[-6], yystack.l_mark[-4]);
  yystack.l_mark[-6].code = join_code(yystack.l_mark[-6].code, tmp);

  goto_s -> label = tmp ->label;
  
  back_patching(yystack.l_mark[0].true_list, tmp->label, NULL);
  
  yystack.l_mark[-6].true_list = merge(if_s, yystack.l_mark[0].break_list);
  
  yystack.l_mark[-6].false_list = NULL;
  yyval = yystack.l_mark[-6];
  
  
  /*  ID := e1
     l1:    e2.code
            if ID.place > e2.place goto l2
        s.code
            ID.place := ID.place + 1
        goto l1
         l2: 
         
         examples: 
          
          for i:=1 to 100 do a:= a+1 .
          ===>
          ##code _listing:
                  i := 1
          l0:     if (i > 100) goto  l1
                  t0 := a + 1
                  a := t0
                  i := i + 1
                  goto l0
          l1:
          ##end_listing
         
         
         */
}
break;
case 11:
#line 649 "control.y"
	{
  yyval = yystack.l_mark[-2];
}
break;
case 12:
#line 653 "control.y"
	{
  yyval = yystack.l_mark[-1];
}
break;
case 13:
#line 657 "control.y"
	{
  ATT as = assign(yystack.l_mark[-2], yystack.l_mark[0]);
  if (yystack.l_mark[-2].code != NULL) as.code = join_code(yystack.l_mark[-2].code, as.code);

  yyval = as;
}
break;
case 14:
#line 664 "control.y"
	{ 
  int is_enum, is_last;

  is_last = (yystack.l_mark[-1].false_list != NULL && 
         (strncmp((char*)yystack.l_mark[-1].false_list, "last", 4) == 0));

  if( is_last ){
    yystack.l_mark[-1].false_list = NULL;
  }

  is_enum = (yystack.l_mark[-1].false_list != NULL && 
         yystack.l_mark[-1].false_list->code != NULL &&
         yystack.l_mark[-1].false_list->code[0] == '_');

  if (is_enum) {
    printf("the action in enumerated cases is empty!\n");
    exit (1);
  }

  yystack.l_mark[-3].code = join_code(yystack.l_mark[-3].code, yystack.l_mark[-1].code);
  yystack.l_mark[-3].true_list = merge(yystack.l_mark[-1].true_list, yystack.l_mark[-1].false_list);
  yystack.l_mark[-3].true_list = merge(yystack.l_mark[-3].true_list, yystack.l_mark[-1].break_list);
  
  yystack.l_mark[-3].continue_list = yystack.l_mark[-1].continue_list;
     /* break is accepted in switch statement just like C switch */    
  yystack.l_mark[-3].false_list = NULL;
    
  yyval = yystack.l_mark[-3];
}
break;
case 15:
#line 710 "control.y"
	{
  yyval = make_att(NULL, NULL, NULL);
}
break;
case 16:
#line 714 "control.y"
	{ /* consecutive "CASE ID" */
  char s [40];
  CODE * goto_s, * if_s, * tmp;
  int is_enum, is_last;

  /* must test if last case first, if not so access 
     false_list failure */

  is_last = (yystack.l_mark[-3].false_list != NULL
         && (strncmp((char*)yystack.l_mark[-3].false_list, "last", 4) == 0));
  
  if (is_last){
    printf("the default case must be last case!\n");
    exit (1);
  }

  is_enum = (yystack.l_mark[-3].false_list != NULL &&       
         yystack.l_mark[-3].false_list->code != NULL &&
         yystack.l_mark[-3].false_list->code[0] == '_');
  
  sprintf(s, "if (%s = %s) goto ", (char *) yystack.l_mark[-5].true_list, 
	  (char *) yystack.l_mark[-1].true_list);
  if_s = make_code(s, NULL);
  tmp = if_s;
  
    /* last case is not consecutive case, 
       backpatching case false link */         
  if (yystack.l_mark[-3].code != NULL && !is_enum ){ 
    tmp = get_first(tmp); 
    back_patching(yystack.l_mark[-3].false_list, tmp->label, NULL);
  }
  
  yystack.l_mark[-3].code = join_code(yystack.l_mark[-3].code, tmp);
  
  if( !is_enum )  {
    yystack.l_mark[-3].false_list = merge(make_code("_", NULL), yystack.l_mark[-3].true_list); 
      /* false_list will remember case out list */
    yystack.l_mark[-3].true_list = if_s;
  } else
    yystack.l_mark[-3].true_list =  merge(yystack.l_mark[-3].true_list, if_s);
  
  yyval = yystack.l_mark[-3];   
    
     /* case v1: case v2:  .... 
        normal false_list pointer  "if (a<>2) goto"
        and true_list link "goto" statement     
        
        in duplicate case: 
            false_list will remember  "goto" statement
            and true_list will pointer list of "if (a=0) goto"
            
               if (a = 0) goto  l0
               if (a = 1) goto  l0
               if (a = 11) goto  l0
               if (a = 12) goto  l0
               if (a <> 2) goto  l1
       l0:     a := 2
               goto l4
       l1:     if (a <> 3) goto  l2
               t0 := c * c
               t1 := b + t0
               a := t1
               goto l4
       l2:     if (a <> 4) goto  l3
               t2 := a + 3
               a := t2
               goto l4
       l3:     t3 := a + d
               a := t3
               
        switch a 
            begin
                  case 1: if b<0 then a:=0 else a:= 1
                  case 2: while a>0 do a:=a+1
                  case 3:
                  case 4:  a:=220
                  case default: a:=1000
            end.
         ===>  
        ##code _listing:
        // case 1
                if (a <> 1) goto  l4
                if (b < 0) goto l0
                goto l1
        l0:     a := 0
                goto l8
        l1:     a := 1
                goto l8
        // case 2
        l4:     if (a <> 2) goto  l5
        l2:     if (a > 0) goto l3
                goto l8
        l3:     t0 := a + 1
                a := t0
                goto l2
                goto l8
        // case 3 4
        l5:     if (a = 3) goto  l6
                if (a <> 4) goto  l7
        l6:     a := 220
                goto l8
        // case default
        l7:     a := 1000
        l8:
        ##end_listing
     */
}
break;
case 17:
#line 822 "control.y"
	{
  char s [40]; 
  CODE * goto_s, * if_s, * tmp;

  int is_enum, is_last;

  is_last = (yystack.l_mark[-4].false_list != NULL 
         && (strncmp((char*)(yystack.l_mark[-4].false_list), "last", 4) == 0));

        /* test if default case is defined before current case */
  if ( is_last ) {
    printf("the default case must be last case!\n");
    exit (1);
  }
  
  is_enum = ( yystack.l_mark[-4].false_list != NULL && 
          yystack.l_mark[-4].false_list -> code != NULL &&
          yystack.l_mark[-4].false_list->code [0] == '_');  

  sprintf(s, "if (%s <> %s) goto ", (char *) yystack.l_mark[-6].true_list, 
	  (char *) yystack.l_mark[-2].true_list);
  if_s = make_code(s, NULL); 
  goto_s = make_code("goto",  NULL);
  
  if (is_enum)  {
    yystack.l_mark[0].code = get_first(yystack.l_mark[0].code);
    back_patching(yystack.l_mark[-4].true_list, yystack.l_mark[0].code ->label, NULL);
  }
  
  tmp = join_code(if_s, yystack.l_mark[0].code);
  tmp = join_code(tmp, goto_s);
  
  if (yystack.l_mark[-4].code != NULL && !is_enum){
    tmp = get_first(tmp);
    back_patching(yystack.l_mark[-4].false_list, tmp->label, NULL);
  }
  
  yystack.l_mark[-4].code = join_code(yystack.l_mark[-4].code, tmp);
  
  if (is_enum)
    yystack.l_mark[-4].true_list = (CODE *) yystack.l_mark[-4].false_list ->label; 
  
  goto_s ->label = (char *) yystack.l_mark[0].true_list;
  yystack.l_mark[-4].true_list = merge(yystack.l_mark[-4].true_list, goto_s);
  
  yystack.l_mark[-4].false_list =  if_s;

  yystack.l_mark[-4].break_list = merge (yystack.l_mark[-4].break_list, yystack.l_mark[0].break_list);
  yystack.l_mark[-4].continue_list = merge(yystack.l_mark[-4].continue_list, yystack.l_mark[0].continue_list);    
  
  yyval = yystack.l_mark[-4];  
}
break;
case 18:
#line 875 "control.y"
	{
  CODE  *tmp = yystack.l_mark[0].code;

  int is_enum, is_last, test;

  is_last = (yystack.l_mark[-4].false_list != NULL && 
         (strncmp((char *) yystack.l_mark[-4].false_list, "last", 4) == 0));
 
  /* test if there is a duplicate default case */
  if (is_last) {
    printf("Duplicated default case!\n");
    exit (1);
  }

  is_enum = (yystack.l_mark[-4].false_list != NULL &&
         yystack.l_mark[-4].false_list->code != NULL &&
         yystack.l_mark[-4].false_list->code [0] == '_');

  test =  (yystack.l_mark[-4].code != NULL && !is_enum);
  
  if ( test ) {
    tmp = get_first(yystack.l_mark[0].code);
    back_patching(yystack.l_mark[-4].false_list, tmp->label, NULL);
  } else {
    if (is_enum) { 
      tmp = get_first(yystack.l_mark[0].code);
      back_patching(yystack.l_mark[-4].true_list, tmp ->label, NULL);  
      yystack.l_mark[-4].true_list =(CODE *) yystack.l_mark[-4].false_list ->label;
    }
  }
        
  yystack.l_mark[-4].code = join_code(yystack.l_mark[-4].code, tmp);
    
  if (yystack.l_mark[0].true_list != NULL) 
    yystack.l_mark[-4].true_list = merge(yystack.l_mark[-4].true_list, yystack.l_mark[0].true_list);
     
  yystack.l_mark[-4].false_list =  (CODE *) last_mark;
      /* a mark of last case */
        
  yystack.l_mark[-4].break_list = merge (yystack.l_mark[-4].break_list, yystack.l_mark[0].break_list);

  yystack.l_mark[-4].continue_list = merge(yystack.l_mark[-4].continue_list, yystack.l_mark[0].continue_list);    
      
  yyval = yystack.l_mark[-4];    
}
break;
case 19:
#line 922 "control.y"
	{
  yyval = yystack.l_mark[0];
}
break;
case 20:
#line 926 "control.y"
	{
  /* array element */
  /*
    a:= a[b, c, d];
    t0 := b * limit(a, 2)
    t0 := t0 + c
    t1 := t0 * limit(a, 3)
    t1 := t1 + d
    t2 := const(array of a)
    t3 := t1 * width of a
    t4 := t2 [t3]
    a := t4 
  */
  int count = 2;
  CODE * ptr  = yystack.l_mark[-1].false_list;
  CODE * code =  ( (CODE *) ptr->label);
  char s[30];
  char * a_name = (char *) yystack.l_mark[-3].true_list;
  char *prev_tmp = ptr->code;
  char * tmp ,*tmp1, *tmp2;

  ptr = ptr->next;
    
  tmp = prev_tmp; /* for one dim */
    
  while(ptr != NULL) {
    char s [30] ;
    tmp = new_name();
    code = join_code(code, (CODE *) ptr->label); 

    sprintf(s, "%s := %s * limit(%s, %d)", tmp, prev_tmp, a_name, count); 
    code = join_code (code, make_code(s, NULL));
       
    prev_tmp = ptr->code;
    sprintf(s, "%s := %s + %s", tmp, tmp, prev_tmp);
    code = join_code (code, make_code(s, NULL));
        
    prev_tmp = tmp;
    ptr = ptr ->next;
    count++;
  }

  /*
    a:=b[c+d, e+f, g+h, i+j].
    ##code _listing:
    t0 := c + d
    t1 := e + f
    t4 := t0 * limit(b, 2)
    t4 := t4 + t1
    t2 := g + h
    t5 := t4 * limit(b, 3)
    t5 := t5 + t2
    t3 := i + j
    t6 := t5 * limit(b, 4)
    t6 := t6 + t3
    t7 := const (array of  b)
    t8 := t6 * width ( b )
    t9 := t7 [ t8 ]
    a := t9

    ##end_listing
  */
        
  tmp1 = new_name();    
  sprintf(s, "%s := const (array of  %s)", tmp1, a_name);
  code = join_code(code, make_code(s, NULL));
    

  tmp2 = new_name();
    
  sprintf(s, "%s := %s * width ( %s )", tmp2, tmp, a_name);
  code = join_code(code, make_code(s, NULL));
    
  tmp = new_name();
  sprintf(s, "%s := %s [ %s ]", tmp, tmp1, tmp2);
  code = join_code(code, make_code(s, NULL));
   
  yystack.l_mark[-3].code = code;
  yystack.l_mark[-3].true_list = (CODE *)tmp;
    
  yyval = yystack.l_mark[-3];
}
break;
case 21:
#line 1010 "control.y"
	{
  char * tmp = new_name();
  char  s[20];
  CODE * code;
  
  sprintf (s, "%s := %s + %s", tmp, (char *) yystack.l_mark[-2].true_list, (char *) yystack.l_mark[0].true_list);
  code  = make_code(s, NULL);
  
  yystack.l_mark[-2].true_list = (CODE *) tmp; 
    /* use true_list to store the tmp value */
  
  yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, yystack.l_mark[0].code);
  yystack.l_mark[-2].code = join_code (yystack.l_mark[-2].code, code);
  yyval = yystack.l_mark[-2];
}
break;
case 22:
#line 1026 "control.y"
	{
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := %s * %s", tmp, (char *) yystack.l_mark[-2].true_list, (char *) yystack.l_mark[0].true_list);
  code  = make_code(s, NULL);
    
  yystack.l_mark[-2].true_list = (CODE *) tmp;
  yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, yystack.l_mark[0].code);
  yystack.l_mark[-2].code = join_code (yystack.l_mark[-2].code, code);
    
  yyval = yystack.l_mark[-2];
}
break;
case 23:
#line 1041 "control.y"
	{
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := %s - %s", tmp, (char *) yystack.l_mark[-2].true_list, (char *) yystack.l_mark[0].true_list);
  code  = make_code(s, NULL);
    
  yystack.l_mark[-2].true_list = (CODE *) tmp;
  yystack.l_mark[-2].code = join_code(yystack.l_mark[-2].code, yystack.l_mark[0].code);
  yystack.l_mark[-2].code = join_code (yystack.l_mark[-2].code, code);
    
  yyval = yystack.l_mark[-2];
}
break;
case 24:
#line 1056 "control.y"
	{
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := - %s", tmp, (char *) yystack.l_mark[0].true_list);
  code  = make_code(s, NULL);
    
  yystack.l_mark[0].true_list = (CODE *) tmp;
  yystack.l_mark[0].code = join_code(yystack.l_mark[0].code, code);
    
  yyval = yystack.l_mark[0];
}
break;
case 25:
#line 1071 "control.y"
	{
  yyval = yystack.l_mark[-1];
}
break;
case 27:
#line 1077 "control.y"
	{
  /* function call */
  int count = 0;
  char * tmp = new_name();
  CODE * param = NULL, * code = NULL;
  CODE * ptr  = yystack.l_mark[-1].false_list;
  char s[30];

  while(ptr != NULL) {
    char s [30] ;
    sprintf(s, "param %s", (ptr)-> code);
    code = join_code(code, (CODE*)ptr->label);
    param = join_code (param, make_code(s, NULL));
    ptr = ptr ->next;
    count++;
  }
        
  sprintf(s, "%s := call %s, %d", tmp, (char *) yystack.l_mark[-3].true_list, count);
  param = join_code(param, make_code(s, NULL));
    
  yystack.l_mark[-3].code = join_code(code, param);
  yystack.l_mark[-3].true_list = (CODE *)tmp;
    
  yyval = yystack.l_mark[-3];
}
break;
case 28:
#line 1122 "control.y"
	{
  /* use code list to link actual parameter,
     and code->code holds the a.p. tmp result. 
     and code->label holds the code chain of evaluation a.p.
     and the list head will store in att.false_list */
          
  yystack.l_mark[0].false_list = make_code((char *)yystack.l_mark[0].true_list, (char *) yystack.l_mark[0].code);
  yyval = yystack.l_mark[0];
}
break;
case 29:
#line 1132 "control.y"
	{
  CODE * e_result = make_code((char *)yystack.l_mark[0].true_list, (char *) yystack.l_mark[0].code);
    
  yystack.l_mark[-2].false_list = join_code(yystack.l_mark[-2].false_list, e_result);

  yyval = yystack.l_mark[-2];
}
break;
case 30:
#line 1141 "control.y"
	{ 
  yyval = combine(yystack.l_mark[-2], yystack.l_mark[0], FILL_FALSE);
  }
break;
case 31:
#line 1145 "control.y"
	{ 
  yyval = combine(yystack.l_mark[-2], yystack.l_mark[0], FILL_TRUE);
}
break;
case 32:
#line 1150 "control.y"
	{
  yyval = translate_not(yystack.l_mark[0]);
}
break;
case 33:
#line 1154 "control.y"
	{
  yyval = yystack.l_mark[-1];
}
break;
case 34:
#line 1158 "control.y"
	{
  char  s [40];
  CODE *code1, *code2;
    
  sprintf(s, "1(%s %s %s)", (char *)yystack.l_mark[-2].true_list, (char *)yystack.l_mark[-1].true_list,
          (char *)yystack.l_mark[0].true_list);
        
  code1 = make_code (s, NULL);
  
  yystack.l_mark[-2].true_list = code1;
  yystack.l_mark[-2].false_list = code1;
  code2 = join_code (yystack.l_mark[0].code, code1);
  yystack.l_mark[-2].code = join_code (yystack.l_mark[-2].code,  code2);
  yyval = yystack.l_mark[-2];
}
break;
case 35:
#line 1174 "control.y"
	{
  CODE * code  = make_code(b_true, NULL);
  ATT  att  = make_att(NULL, NULL, code);
  yyval = att;
}
break;
case 36:
#line 1180 "control.y"
	{
  CODE * code  = make_code(b_false, NULL);
  ATT att  = make_att(NULL, NULL, code);
  yyval = att;
}
break;
#line 1744 "y.tab.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            yychar = YYLEX;
            if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
        goto yyloop;
    }
    if (((yyn = yygindex[yym]) != 0) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    YYERROR_CALL("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
