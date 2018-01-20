%{
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
%}

%token IF THEN ELSE WHILE DO SBEGIN END  ID ASSIGN BREAK CONTINUE
%token AND OR NOT TRUE FALSE RELOP FOR TO CASE SWITCH DEFAULT
%token REPEAT UNTIL

%left '+' '-'
%left '*'
%left UMINUS
%left OR
%left AND
%left NOT
%left ELSE

%%
p : s_list '.' {
  if($1.true_list != NULL) {
    CODE * label_s = make_code(NULL, new_label());
    $1.code = join_code($1.code, label_s);
    back_patching($1.true_list, label_s->label, NULL);
  }
       
  if ($1.break_list != NULL)
    printf("Error: break statement misplaced!\n");
  
  if ($1.continue_list != NULL)
    printf("Error: continue statement misplaced!\n");
  
  print_code($1.code);
}
;

s_list  :  s 
|  s_list ';' s {
    CODE * code = $3.code;
    if ($1.true_list != NULL ) {
      code = get_first ($3.code);
      back_patching($1.true_list, code->label, NULL);
    }  
    
    $1.true_list = $3.true_list;   /* not break */
    
    $1.break_list = merge($1.break_list, $3.break_list);        
    $1.continue_list = merge($1.continue_list, $3.continue_list);        
    
    $1.code = join_code($1.code, code);
    
    $$ = $1;
}
;

s : BREAK {
    CODE * goto_s = make_code("goto", NULL);
    ATT att = make_att( NULL, NULL,goto_s);
    att.break_list = goto_s;
    $$ = att;
    
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
    
| CONTINUE {
  CODE * goto_s = make_code("goto", NULL);
  ATT att = make_att( NULL, NULL,goto_s);
  att.continue_list = goto_s;
  $$ = att;
}

| IF b_e THEN s  {
  $$ = translate_if_then($2, $4);
}
    
| IF b_e THEN s ELSE s {
  $$ = translate_if_then_else($2, $4, $6);
}

|  REPEAT s_list UNTIL b_e {
  $$ = translate_repeat($2, $4);
 }
 
    /*
        l:  s.code
            b_e.code
            ( b_e.true_list to next
              b_e.false_list to l )
             
        example: repeat a := a+b until a>0.
        ==> 
	##code _listing:
	l0:	t0 := a + b
		a := t0
		ifnot (a > 0) goto  l0 

	##end_listing
     */ 
   
|  WHILE  b_e DO s {
  $$ = translate_while($2, $4);
} 
        
|  FOR ID ASSIGN e TO e DO s {
  CODE * goto_s = make_code ("goto", NULL);
  char s[40];
  CODE * if_s,  *increment, *tmp;
  
  sprintf(s, "if (%s > %s) goto ", (char *) $2.true_list, 
	  (char *) $6.true_list);
  if_s = make_code (s, NULL);
  
  sprintf(s, "%s := %s + 1", (char *) $2.true_list, (char *) $2.true_list);
  increment = make_code (s, NULL);
  
  increment -> next = goto_s;
  
  $6.code = join_code ($6.code, if_s);
  $6.code = join_code($6.code, $8.code);
  $6.code = join_code($6.code, increment);
  
  tmp = get_first($6.code);
  
  back_patching($8.continue_list, tmp->label, NULL);
   /* backpatching continue list */
  
  $2 = assign($2, $4);
  $2.code = join_code($2.code, tmp);

  goto_s -> label = tmp ->label;
  
  back_patching($8.true_list, tmp->label, NULL);
  
  $2.true_list = merge(if_s, $8.break_list);
  
  $2.false_list = NULL;
  $$ = $2;
  
  
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
    
|  SBEGIN s_list ';' END {
  $$ = $2;
}
   
|  SBEGIN s_list  END {
  $$ = $2;
}
 
|  l ASSIGN e {
  ATT as = assign($1, $3);
  if ($1.code != NULL) as.code = join_code($1.code, as.code);

  $$ = as;
}

|  SWITCH e SBEGIN case_list  END { 
  int is_enum, is_last;

  is_last = ($4.false_list != NULL && 
         (strncmp((char*)$4.false_list, "last", 4) == 0));

  if( is_last ){
    $4.false_list = NULL;
  }

  is_enum = ($4.false_list != NULL && 
         $4.false_list->code != NULL &&
         $4.false_list->code[0] == '_');

  if (is_enum) {
    printf("the action in enumerated cases is empty!\n");
    exit (1);
  }

  $2.code = join_code($2.code, $4.code);
  $2.true_list = merge($4.true_list, $4.false_list);
  $2.true_list = merge($2.true_list, $4.break_list);
  
  $2.continue_list = $4.continue_list;
     /* break is accepted in switch statement just like C switch */    
  $2.false_list = NULL;
    
  $$ = $2;
}
;
        /* 
                    e.code
                    if e.place <> V1 goto l1
                    s1.code
                    goto next
           l1:      if e.place <> V2 goto l2
                    s2.code
                    ...
           lp:      if e.place <> Vp+1 goto  lp+1
                    sp+1.code
                    goto next
           lp+1:    default.code 
           next: 
       */


case_list :   {
  $$ = make_att(NULL, NULL, NULL);
}   /* epsilon */

| case_list  CASE ID ':' { /* consecutive "CASE ID" */
  char s [40];
  CODE * goto_s, * if_s, * tmp;
  int is_enum, is_last;

  /* must test if last case first, if not so access 
     false_list failure */

  is_last = ($1.false_list != NULL
         && (strncmp((char*)$1.false_list, "last", 4) == 0));
  
  if (is_last){
    printf("the default case must be last case!\n");
    exit (1);
  }

  is_enum = ($1.false_list != NULL &&       
         $1.false_list->code != NULL &&
         $1.false_list->code[0] == '_');
  
  sprintf(s, "if (%s = %s) goto ", (char *) $-1.true_list, 
	  (char *) $3.true_list);
  if_s = make_code(s, NULL);
  tmp = if_s;
  
    /* last case is not consecutive case, 
       backpatching case false link */         
  if ($1.code != NULL && !is_enum ){ 
    tmp = get_first(tmp); 
    back_patching($1.false_list, tmp->label, NULL);
  }
  
  $1.code = join_code($1.code, tmp);
  
  if( !is_enum )  {
    $1.false_list = merge(make_code("_", NULL), $1.true_list); 
      /* false_list will remember case out list */
    $1.true_list = if_s;
  } else
    $1.true_list =  merge($1.true_list, if_s);
  
  $$ = $1;   
    
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

|  case_list  CASE ID ':' s_list {
  char s [40]; 
  CODE * goto_s, * if_s, * tmp;

  int is_enum, is_last;

  is_last = ($1.false_list != NULL 
         && (strncmp((char*)($1.false_list), "last", 4) == 0));

        /* test if default case is defined before current case */
  if ( is_last ) {
    printf("the default case must be last case!\n");
    exit (1);
  }
  
  is_enum = ( $1.false_list != NULL && 
          $1.false_list -> code != NULL &&
          $1.false_list->code [0] == '_');  

  sprintf(s, "if (%s <> %s) goto ", (char *) $-1.true_list, 
	  (char *) $3.true_list);
  if_s = make_code(s, NULL); 
  goto_s = make_code("goto",  NULL);
  
  if (is_enum)  {
    $5.code = get_first($5.code);
    back_patching($1.true_list, $5.code ->label, NULL);
  }
  
  tmp = join_code(if_s, $5.code);
  tmp = join_code(tmp, goto_s);
  
  if ($1.code != NULL && !is_enum){
    tmp = get_first(tmp);
    back_patching($1.false_list, tmp->label, NULL);
  }
  
  $1.code = join_code($1.code, tmp);
  
  if (is_enum)
    $1.true_list = (CODE *) $1.false_list ->label; 
  
  goto_s ->label = (char *) $5.true_list;
  $1.true_list = merge($1.true_list, goto_s);
  
  $1.false_list =  if_s;

  $1.break_list = merge ($1.break_list, $5.break_list);
  $1.continue_list = merge($1.continue_list, $5.continue_list);    
  
  $$ = $1;  
}
    
| case_list  CASE DEFAULT ':' s_list  {
  CODE  *tmp = $5.code;

  int is_enum, is_last, test;

  is_last = ($1.false_list != NULL && 
         (strncmp((char *) $1.false_list, "last", 4) == 0));
 
  /* test if there is a duplicate default case */
  if (is_last) {
    printf("Duplicated default case!\n");
    exit (1);
  }

  is_enum = ($1.false_list != NULL &&
         $1.false_list->code != NULL &&
         $1.false_list->code [0] == '_');

  test =  ($1.code != NULL && !is_enum);
  
  if ( test ) {
    tmp = get_first($5.code);
    back_patching($1.false_list, tmp->label, NULL);
  } else {
    if (is_enum) { 
      tmp = get_first($5.code);
      back_patching($1.true_list, tmp ->label, NULL);  
      $1.true_list =(CODE *) $1.false_list ->label;
    }
  }
        
  $1.code = join_code($1.code, tmp);
    
  if ($5.true_list != NULL) 
    $1.true_list = merge($1.true_list, $5.true_list);
     
  $1.false_list =  (CODE *) last_mark;
      /* a mark of last case */
        
  $1.break_list = merge ($1.break_list, $5.break_list);

  $1.continue_list = merge($1.continue_list, $5.continue_list);    
      
  $$ = $1;    
}
;

l :  ID {
  $$ = $1;
}
    
| ID '[' e_list ']' {
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
  CODE * ptr  = $3.false_list;
  CODE * code =  ( (CODE *) ptr->label);
  char s[30];
  char * a_name = (char *) $1.true_list;
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
   
  $1.code = code;
  $1.true_list = (CODE *)tmp;
    
  $$ = $1;
}
;    
    
e : e '+' e {
  char * tmp = new_name();
  char  s[20];
  CODE * code;
  
  sprintf (s, "%s := %s + %s", tmp, (char *) $1.true_list, (char *) $3.true_list);
  code  = make_code(s, NULL);
  
  $1.true_list = (CODE *) tmp; 
    /* use true_list to store the tmp value */
  
  $1.code = join_code($1.code, $3.code);
  $1.code = join_code ($1.code, code);
  $$ = $1;
}

|  e '*' e {
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := %s * %s", tmp, (char *) $1.true_list, (char *) $3.true_list);
  code  = make_code(s, NULL);
    
  $1.true_list = (CODE *) tmp;
  $1.code = join_code($1.code, $3.code);
  $1.code = join_code ($1.code, code);
    
  $$ = $1;
}

|  e '-' e {
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := %s - %s", tmp, (char *) $1.true_list, (char *) $3.true_list);
  code  = make_code(s, NULL);
    
  $1.true_list = (CODE *) tmp;
  $1.code = join_code($1.code, $3.code);
  $1.code = join_code ($1.code, code);
    
  $$ = $1;
}

| '-' e %prec UMINUS {
  char * tmp = new_name();
  char  s[20];
  CODE * code ;
    
  sprintf (s, "%s := - %s", tmp, (char *) $2.true_list);
  code  = make_code(s, NULL);
    
  $2.true_list = (CODE *) tmp;
  $2.code = join_code($2.code, code);
    
  $$ = $2;
}
  

| '(' e ')' {
  $$ = $2;
}

| l

| ID '(' e_list ')' {
  /* function call */
  int count = 0;
  char * tmp = new_name();
  CODE * param = NULL, * code = NULL;
  CODE * ptr  = $3.false_list;
  char s[30];

  while(ptr != NULL) {
    char s [30] ;
    sprintf(s, "param %s", (ptr)-> code);
    code = join_code(code, (CODE*)ptr->label);
    param = join_code (param, make_code(s, NULL));
    ptr = ptr ->next;
    count++;
  }
        
  sprintf(s, "%s := call %s, %d", tmp, (char *) $1.true_list, count);
  param = join_code(param, make_code(s, NULL));
    
  $1.code = join_code(code, param);
  $1.true_list = (CODE *)tmp;
    
  $$ = $1;
}
/*
  a:=b(a*c,e-f, -d) * 300 -100.
  ==>
  ##code _listing:
  t0 := a * c
  t1 := e - f
  t2 := - d
  param t0
  param t1
  param t2
  t3 := call b, 3
  t4 := t3 * 300
  t5 := t4 - 100
  a := t5

  ##end_listing
*/
  
;

e_list : e  {
  /* use code list to link actual parameter,
     and code->code holds the a.p. tmp result. 
     and code->label holds the code chain of evaluation a.p.
     and the list head will store in att.false_list */
          
  $1.false_list = make_code((char *)$1.true_list, (char *) $1.code);
  $$ = $1;
}
          
| e_list ',' e {
  CODE * e_result = make_code((char *)$3.true_list, (char *) $3.code);
    
  $1.false_list = join_code($1.false_list, e_result);

  $$ = $1;
}
;

b_e : b_e OR b_e { 
  $$ = combine($1, $3, FILL_FALSE);
  }
    
| b_e AND b_e { 
  $$ = combine($1, $3, FILL_TRUE);
}


| NOT b_e {
  $$ = translate_not($2);
}

| '(' b_e ')' {
  $$ = $2;
}

| e RELOP e {
  char  s [40];
  CODE *code1, *code2;
    
  sprintf(s, "1(%s %s %s)", (char *)$1.true_list, (char *)$2.true_list,
          (char *)$3.true_list);
        
  code1 = make_code (s, NULL);
  
  $1.true_list = code1;
  $1.false_list = code1;
  code2 = join_code ($3.code, code1);
  $1.code = join_code ($1.code,  code2);
  $$ = $1;
}

| TRUE {
  CODE * code  = make_code(b_true, NULL);
  ATT  att  = make_att(NULL, NULL, code);
  $$ = att;
}

| FALSE {
  CODE * code  = make_code(b_false, NULL);
  ATT att  = make_att(NULL, NULL, code);
  $$ = att;
}
;

    
%%
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
