%{
/* control flow translation using backpatching 
	(C) hanfei.wang@gmail.com  
         2011.12.16      			*/
#ifdef GCC
#include "y.tab.h"
#else
#include "y_tab.h"
#endif

typedef struct code {
	char * code;
        char * label;
        struct code * next;
	} CODE;
 
typedef struct att {
     CODE * true_list;
     CODE * false_list;
     CODE * code;
     CODE * break_list;   /* add for the break statement */ 
     CODE * continue_list;  /* add for continue statement */
} ATT;

extern ATT yylval;
ATT make_att(CODE * tl, CODE * fl, CODE * code);

%}
%%
(\t|" "|\n|\r)+   ;
":="    return ASSIGN;
">" |
">=" |
"<" | 
"<=" |
"=" 	{ 
	char * s = (char *) malloc(strlen(yytext) +1);
       	strcpy(s, yytext);
        yylval =make_att((CODE *) s, NULL, NULL);
        return RELOP;
        }
"repeat" 	return REPEAT;
"until" 	return UNTIL;
"break"		return BREAK;
"continue"	return CONTINUE;
"case"		return CASE;
"switch"	return SWITCH;
"default"	return DEFAULT;
"for"		return FOR;
"to" 		return TO;
"if" 		return IF;
"then" 		return THEN;
"else" 		return ELSE;
"while" 	return WHILE;
"do" 		return DO;
"begin" 	return SBEGIN;
"end"		return END;
"and"		return AND;
"or"		return OR;
"not" 		return NOT;
"true" 		return TRUE;
"false" 	return FALSE;

[0-9]+|[a-z]+  { 
	char * s = (char *) malloc(strlen(yytext) +1);
       	strcpy(s, yytext);
        yylval =  make_att((CODE *) s, NULL, NULL);
        return ID;
        }

. 	return yytext[0];

%%

int yywrap(void)
{
  return 1;
}
