/* Traductor educativo C -> JavaScript: lexer, parser y comprobacion de tipos.
   Subconjunto: int, double, char ASCII, printf, if/else, for, while, main.
   Las estructuras no admitidas se rechazan; no se traducen por reemplazos. */
#include "traductor.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <math.h>
#define MAXTOK 20000
#define MAXSYM 512
#define MAXOUT 1048576
#define MAXMEM 33554432
#define NUM 1
#define ID 2
#define STR 3
#define CHR 4
#define PUN 5
#define END 6
#define TINT 1
#define TDOUBLE 2
#define TCHAR 3
#define TSTRING 4

typedef struct Mem { void *ptr; struct Mem *next; } Mem;
typedef struct { int kind,line,col; char *text; } Tok;
typedef struct { char *name,*js; int type,scope,initialized; } Sym;
typedef struct { char *code; int type; Tok *at; } Expr;
typedef struct {
    Tok toks[MAXTOK]; int nt,pos; Sym symbols[MAXSYM]; int ns,serial,scope;
    int loop,depth,in_main,stmtdepth; char *out; size_t len,allocated; Mem *mem;
    jmp_buf escape; ErrorTraduccion error;
} Ctx;
static void fail(Ctx *c, Tok *t, const char *phase, const char *format, ...) {
    va_list ap;
    c->error.linea=t?t->line:1; c->error.columna=t?t->col:1;
    snprintf(c->error.fase,sizeof(c->error.fase),"%s",phase);
    va_start(ap,format); vsnprintf(c->error.mensaje,sizeof(c->error.mensaje),format,ap); va_end(ap);
    longjmp(c->escape,1);
}
static void *alloc(Ctx *c,size_t n) {
    Mem *m; void *p;
    if(n>MAXMEM || c->allocated>MAXMEM-n) fail(c,NULL,"Limite","Programa demasiado complejo.");
    p=calloc(1,n); m=(Mem*)malloc(sizeof(Mem));
    if(!p||!m) {free(p);free(m);fail(c,NULL,"Memoria","No hay memoria suficiente.");}
    m->ptr=p;m->next=c->mem;c->mem=m;c->allocated+=n;return p;
}
static char *slice(Ctx *c,const char *p,size_t n){char *s=(char*)alloc(c,n+1);memcpy(s,p,n);return s;}
static char *fmt(Ctx *c,const char *format,...) {
    va_list ap; int n; char *s;
    va_start(ap,format); n=vsnprintf(NULL,0,format,ap); va_end(ap);
    if(n<0)fail(c,NULL,"Interno","Error de formato.");
    s=(char*)alloc(c,(size_t)n+1);
    va_start(ap,format);vsnprintf(s,(size_t)n+1,format,ap);va_end(ap);return s;
}
static void emit(Ctx *c,int indent,const char *format,...) {
    va_list ap; int n,j; size_t room;
    if(indent<0 || indent>128)fail(c,NULL,"Limite","Demasiados bloques anidados.");
    if(c->len+(size_t)indent*4+2>=MAXOUT)fail(c,NULL,"Limite","Salida demasiado grande.");
    for(j=0;j<indent*4;j++)c->out[c->len++]=' ';
    room=MAXOUT-c->len;va_start(ap,format);n=vsnprintf(c->out+c->len,room,format,ap);va_end(ap);
    if(n<0||(size_t)n+2>=room)fail(c,NULL,"Limite","Salida demasiado grande.");
    c->len+=(size_t)n;c->out[c->len++]='\n';c->out[c->len]=0;
}
static void addtok(Ctx *c,int kind,const char *s,size_t n,int line,int col){
    Tok *t;if(c->nt>=MAXTOK-1)fail(c,NULL,"Limite","Demasiados tokens.");
    t=&c->toks[c->nt++];t->kind=kind;t->text=slice(c,s,n);t->line=line;t->col=col;
}
static int escape_value(Ctx *c,int v,Tok *t) {
    switch(v){case 'n':return 10;case 'r':return 13;case 't':return 9;case 'b':return 8;
    case 'f':return 12;case 'v':return 11;case 'a':return 7;case '0':return 0;
    case '\\':return 92;case '\'':return 39;case '"':return 34;case '?':return 63;}
    fail(c,t,"Lexico","Escape no admitido. Use escapes simples de C.");return 0;
}
static char *quote(Ctx *c,const unsigned char *s,size_t n) {
    char *out=(char*)alloc(c,n*6+3);size_t i,k=0;out[k++]='"';
    for(i=0;i<n;i++){unsigned char ch=s[i];
        if(ch=='"'||ch=='\\'){out[k++]='\\';out[k++]=(char)ch;}
        else if(ch<32){snprintf(out+k,7,"\\u%04x",(unsigned)ch);k+=6;}
        else out[k++]=(char)ch;
    }out[k++]='"';out[k]=0;return out;
}
static void lex(Ctx *c,const char *s) {
    size_t i=0,start; int line=1,col=1,l,k;const char *ops[]={"==","!=","<=",">=","&&","||","++","--","+=","-=","*=","/=","%=",NULL};
#define ADV() do {if(s[i]=='\n'){line++;col=1;}else if(((unsigned char)s[i]&0xC0)!=0x80)col++;i++;}while(0)
    while(s[i]){
        unsigned char ch=(unsigned char)s[i];
        if(isspace(ch)){ADV();continue;}start=i;l=line;k=col;
        if(s[i]=='#'){
            if(c->nt&&c->toks[c->nt-1].line==line){Tok t={PUN,l,k,NULL};fail(c,&t,"Lexico","La directiva debe iniciar su propia linea.");}
            char compact[128];size_t z=0;
            while(s[i]&&s[i]!='\n'){if(!isspace((unsigned char)s[i])){if(z>=sizeof(compact)-1)fail(c,NULL,"Lexico","Directiva demasiado larga.");compact[z++]=s[i];}ADV();}
            compact[z]=0;
            if(strcmp(compact,"#include<stdio.h>")&&strcmp(compact,"#include<math.h>")){
                Tok t={PUN,l,k,NULL};fail(c,&t,"Lexico","Solo se admiten #include <stdio.h> y <math.h>.");
            }continue;
        }
        if(s[i]=='/'&&s[i+1]=='/'){while(s[i]&&s[i]!='\n')ADV();continue;}
        if(s[i]=='/'&&s[i+1]=='*'){
            ADV();ADV();while(s[i]&&!(s[i]=='*'&&s[i+1]=='/'))ADV();
            if(!s[i]){Tok t={PUN,l,k,NULL};fail(c,&t,"Lexico","Comentario sin cierre */.");}ADV();ADV();continue;
        }
        if(isalpha(ch)||ch=='_'){while(isalnum((unsigned char)s[i])||s[i]=='_')ADV();addtok(c,ID,s+start,i-start,l,k);continue;}
        if(isdigit(ch)){
            if(s[i]=='0'&&isdigit((unsigned char)s[i+1])){Tok t={NUM,l,k,NULL};fail(c,&t,"Lexico","No se admiten numeros octales ni ceros iniciales.");}
            while(isdigit((unsigned char)s[i]))ADV();
            if(s[i]=='.'){ADV();while(isdigit((unsigned char)s[i]))ADV();}
            if(s[i]=='e'||s[i]=='E'){ADV();if(s[i]=='+'||s[i]=='-')ADV();if(!isdigit((unsigned char)s[i])){Tok t={NUM,l,k,NULL};fail(c,&t,"Lexico","Exponente incompleto.");}while(isdigit((unsigned char)s[i]))ADV();}
            addtok(c,NUM,s+start,i-start,l,k);continue;
        }
        if(ch=='"'||ch=='\''){
            int closing=ch;size_t n=0;unsigned char *decoded=(unsigned char*)alloc(c,strlen(s+i)+1);Tok t={PUN,l,k,NULL};ADV();
            while(s[i]&&s[i]!=closing){int v=(unsigned char)s[i];
                if(v=='\n'||v=='\r')fail(c,&t,"Lexico","Literal sin cierre.");
                if(v=='\\'){ADV();if(!s[i])fail(c,&t,"Lexico","Escape sin cierre.");v=escape_value(c,(unsigned char)s[i],&t);}
                if(closing=='"'&&v==0)fail(c,&t,"Lexico","No se admite \\0 dentro de cadenas.");
                decoded[n++]=(unsigned char)v;ADV();
            }
            if(!s[i])fail(c,&t,"Lexico","Literal sin cierre.");
            ADV();
            if(closing=='\''){
                if(n!=1||decoded[0]>127)fail(c,&t,"Lexico","char requiere un caracter ASCII.");
                addtok(c,CHR,"",0,l,k);c->toks[c->nt-1].text=fmt(c,"%u",(unsigned)decoded[0]);
            }else{addtok(c,STR,"",0,l,k);c->toks[c->nt-1].text=quote(c,decoded,n);}
            continue;
        }
        {int j,found=0;for(j=0;ops[j];j++){if(s[i]==ops[j][0]&&s[i+1]==ops[j][1]){ADV();ADV();addtok(c,PUN,s+start,2,l,k);found=1;break;}}if(found)continue;}
        if(strchr("{}();,+-*/%<>=!",ch)){ADV();addtok(c,PUN,s+start,1,l,k);continue;}
        {Tok t={PUN,l,k,NULL};fail(c,&t,"Lexico","Simbolo no admitido: '%c'.",ch);}
    }addtok(c,END,"<fin>",5,line,col);
#undef ADV
}
static Tok *now(Ctx *c){return &c->toks[c->pos];}
static int at(Ctx *c,const char *s){return !strcmp(now(c)->text,s);}
static int take(Ctx *c,const char *s){if(at(c,s)){c->pos++;return 1;}return 0;}
static void expect(Ctx *c,const char *s){if(!take(c,s))fail(c,now(c),"Sintaxis","Se esperaba '%s'; se encontro '%s'.",s,now(c)->text);}
static int typeword(const char *s){if(!strcmp(s,"int"))return TINT;if(!strcmp(s,"double"))return TDOUBLE;if(!strcmp(s,"char"))return TCHAR;return 0;}
static int reserved(const char *s){
    const char *words[]={"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert","_Thread_local","main","printf","sqrt","pow","fabs","floor","ceil",NULL};int i;
    for(i=0;words[i];i++)if(!strcmp(words[i],s))return 1;
    return 0;
}
static Tok *ident(Ctx *c){Tok *t=now(c);if(t->kind!=ID||reserved(t->text))fail(c,t,"Sintaxis","Se esperaba un nombre de variable valido.");c->pos++;return t;}
static void numeric(Ctx *c,Expr e){if(e.type==TSTRING)fail(c,e.at,"Semantica","La operacion requiere un numero.");}
static Sym *find(Ctx *c,Tok *t){int i;for(i=c->ns-1;i>=0;i--)if(!strcmp(c->symbols[i].name,t->text)){if(!c->symbols[i].initialized)fail(c,t,"Semantica","Variable usada antes de inicializar: %s.",t->text);return &c->symbols[i];}fail(c,t,"Semantica","Variable '%s' no declarada.",t->text);return NULL;}
static Sym *declare(Ctx *c,Tok *t,int type){int i;Sym *s;
    for(i=c->ns-1;i>=0&&c->symbols[i].scope==c->scope;i--)if(!strcmp(c->symbols[i].name,t->text))fail(c,t,"Semantica","Variable '%s' duplicada en este bloque.",t->text);
    if(c->ns>=MAXSYM)fail(c,t,"Limite","Demasiadas variables activas.");
    s=&c->symbols[c->ns++];s->name=t->text;s->type=type;s->scope=c->scope;s->js=fmt(c,"v%d_%s",c->serial++,t->text);return s;
}
static char *convert(Ctx *c,int type,Expr e){numeric(c,e);if(type==TDOUBLE)return e.code;return fmt(c,"%s(%s)",type==TCHAR?"__char":"__int",e.code);}
static Expr expr(Ctx *c,int min);
static Expr make(char *code,int type,Tok *t){Expr e;e.code=code;e.type=type;e.at=t;return e;}
static Expr primary(Ctx *c){Tok *t=now(c);Expr e;
    if(take(c,"(")){e=expr(c,0);expect(c,")");e.code=fmt(c,"(%s)",e.code);return e;}
    if(t->kind==NUM){double v=strtod(t->text,NULL);int ty=strpbrk(t->text,".eE")?TDOUBLE:TINT;c->pos++;
        if(!isfinite(v)||(ty==TINT&&v>2147483647.0))fail(c,t,"Semantica","Literal numerico fuera de rango.");
        return make(t->text,ty,t);}
    if(t->kind==STR||t->kind==CHR){c->pos++;return make(t->text,t->kind==STR?TSTRING:TCHAR,t);}
    if(t->kind==ID){
        if(!strcmp(t->text,"sqrt")||!strcmp(t->text,"pow")||!strcmp(t->text,"fabs")||!strcmp(t->text,"floor")||!strcmp(t->text,"ceil")){
            Expr a,b;int ispow=!strcmp(t->text,"pow");const char *method=!strcmp(t->text,"fabs")?"abs":t->text;c->pos++;expect(c,"(");a=expr(c,0);numeric(c,a);
            if(ispow){expect(c,",");b=expr(c,0);numeric(c,b);expect(c,")");return make(fmt(c,"Math.%s(%s, %s)",method,a.code,b.code),TDOUBLE,t);}
            expect(c,")");return make(fmt(c,"Math.%s(%s)",method,a.code),TDOUBLE,t);
        }else{Sym *s;c->pos++;s=find(c,t);return make(s->js,s->type,t);}
    }fail(c,t,"Sintaxis","Expresion no admitida: '%s'.",t->text);return make(NULL,0,t);
}
static Expr unary(Ctx *c){Tok *t=now(c);Expr e;char *code;
    if(++c->depth>128)fail(c,t,"Limite","Expresion demasiado anidada.");
    if(at(c,"!")||at(c,"+")||at(c,"-")){
        c->pos++;
        if(!strcmp(t->text,"-")&&now(c)->kind==NUM&&!strcmp(now(c)->text,"2147483648")){c->pos++;c->depth--;return make("-2147483648",TINT,t);}
        e=unary(c);numeric(c,e);
        if(!strcmp(t->text,"!"))e=make(fmt(c,"(__truth(%s) ? 0 : 1)",e.code),TINT,t);
        else{code=fmt(c,"(%s%s)",t->text,e.code);e=make(e.type==TDOUBLE?code:fmt(c,"__int(%s)",code),e.type==TDOUBLE?TDOUBLE:TINT,t);}
    }else e=primary(c);c->depth--;return e;
}
static int prec(const char *s){if(!strcmp(s,"||"))return 1;if(!strcmp(s,"&&"))return 2;if(!strcmp(s,"==")||!strcmp(s,"!="))return 3;if(!strcmp(s,"<")||!strcmp(s,">")||!strcmp(s,"<=")||!strcmp(s,">="))return 4;if(!strcmp(s,"+")||!strcmp(s,"-"))return 5;if(!strcmp(s,"*")||!strcmp(s,"/")||!strcmp(s,"%"))return 6;return 0;}
static Expr binary(Ctx *c,Expr a,const char *op,Expr b,Tok *t){int p=prec(op),ty;char *code;numeric(c,a);numeric(c,b);
    if(p<=2)return make(fmt(c,"((__truth(%s) %s __truth(%s)) ? 1 : 0)",a.code,op,b.code),TINT,t);
    if(p<=4){const char *js=!strcmp(op,"==")?"===":!strcmp(op,"!=")?"!==":op;return make(fmt(c,"((%s %s %s) ? 1 : 0)",a.code,js,b.code),TINT,t);}
    ty=a.type==TDOUBLE||b.type==TDOUBLE?TDOUBLE:TINT;
    if(!strcmp(op,"%")&&ty==TDOUBLE)fail(c,t,"Semantica","El operador %% requiere enteros.");
    if(!strcmp(op,"/")&&ty==TINT)code=fmt(c,"__div(%s, %s)",a.code,b.code);
    else if(!strcmp(op,"%"))code=fmt(c,"__mod(%s, %s)",a.code,b.code);
    else {code=fmt(c,"(%s %s %s)",a.code,op,b.code);if(ty==TINT)code=fmt(c,"__int(%s)",code);}
    return make(code,ty,t);
}
static Expr expr(Ctx *c,int min){Expr a=unary(c);while(prec(now(c)->text)>=min&&prec(now(c)->text)>0){Tok *op=now(c);Expr b;c->pos++;b=expr(c,prec(op->text)+1);a=binary(c,a,op->text,b,op);}return a;}
static char *declaration(Ctx *c){int ty=typeword(now(c)->text);Tok *name;Sym *s;Expr e;
    c->pos++;name=ident(c);expect(c,"=");s=declare(c,name,ty);s->initialized=0;e=expr(c,0);numeric(c,e);s->initialized=1;return fmt(c,"let %s = %s",s->js,convert(c,ty,e));
}
static char *update(Ctx *c){Tok *name=ident(c),*op;Sym *s=find(c,name);Expr a=make(s->js,s->type,name),e;op=now(c);c->pos++;
    if(!strcmp(op->text,"++")||!strcmp(op->text,"--")){e=binary(c,a,op->text[0]=='+'?"+":"-",make("1",TINT,op),op);return fmt(c,"%s = %s",s->js,convert(c,s->type,e));}
    if(strcmp(op->text,"=")&&strcmp(op->text,"+=")&&strcmp(op->text,"-=")&&strcmp(op->text,"*=")&&strcmp(op->text,"/=")&&strcmp(op->text,"%="))fail(c,op,"Sintaxis","Se esperaba asignacion, ++ o --.");
    e=expr(c,0);if(strcmp(op->text,"=")){char one[2]={op->text[0],0};e=binary(c,a,one,e,op);}return fmt(c,"%s = %s",s->js,convert(c,s->type,e));
}
static void statement(Ctx *c,int level);
static void block(Ctx *c,int level){int saved=c->ns;expect(c,"{");c->scope++;
    if(c->scope>128)fail(c,now(c),"Limite","Demasiados bloques.");
    while(!at(c,"}")){if(now(c)->kind==END)fail(c,now(c),"Sintaxis","Falta una llave de cierre }.");statement(c,level);}expect(c,"}");c->ns=saved;c->scope--;
}
/* Decodifica la cadena JSON creada por el lexer para validar printf. */
static char *decode(Ctx *c,Tok *t){const char *p=t->text+1;char *d=(char*)alloc(c,strlen(p)+1);size_t n=0;
    while(*p&&*p!='"'){if(*p=='\\'){p++;if(*p=='u'){unsigned v=0;sscanf(p+1,"%4x",&v);d[n++]=(char)v;p+=5;}else d[n++]=*p++;}else d[n++]=*p++;}d[n]=0;return d;
}
static void printf_stmt(Ctx *c,int level){Tok *format;Expr args[64];int n=0,want=0;char *decoded;size_t i;
    expect(c,"(");format=now(c);if(format->kind!=STR)fail(c,format,"Sintaxis","printf requiere una cadena literal de formato.");c->pos++;
    while(take(c,",")){if(n>=64)fail(c,now(c),"Limite","Demasiados argumentos printf.");args[n++]=expr(c,0);}expect(c,")");expect(c,";");decoded=decode(c,format);
    for(i=0;decoded[i];i++)if(decoded[i]=='%'){
        int precision=0;char spec; i++;if(decoded[i]=='%')continue;
        if(decoded[i]=='.'){i++;if(!isdigit((unsigned char)decoded[i]))fail(c,format,"Semantica","Precision de printf invalida.");while(isdigit((unsigned char)decoded[i])){precision=precision*10+(decoded[i]-'0');if(precision>20)fail(c,format,"Semantica","Precision maxima: 20.");i++;}if(decoded[i]!='f')fail(c,format,"Semantica","La precision solo se admite con %%f.");}
        spec=decoded[i];if(!spec||!strchr("difsc",spec))fail(c,format,"Semantica","Formato no admitido. Use %%d, %%i, %%f, %%s, %%c o %%%%.");
        if(want>=n)fail(c,format,"Semantica","Faltan argumentos para printf.");
        if(spec=='s'){if(args[want].type!=TSTRING)fail(c,args[want].at,"Semantica","%%s requiere una cadena literal.");}
        else {numeric(c,args[want]);if(spec=='f'&&args[want].type!=TDOUBLE)fail(c,args[want].at,"Semantica","%%f requiere double (por ejemplo 2.0).");if(spec!='f'&&args[want].type==TDOUBLE)fail(c,args[want].at,"Semantica","Formato entero incompatible con double.");}
        want++;
    }
    if(want!=n)fail(c,format,"Semantica","Sobran argumentos para printf en este subconjunto.");
    {char *list=slice(c,"",0);int j;for(j=0;j<n;j++)list=fmt(c,"%s%s%s",list,j?", ":"",args[j].code);emit(c,level,"__printf(%s, [%s]);",format->text,list);}
}
static void parse_statement(Ctx *c,int level){Expr e;char *code;
    if(take(c,";")){emit(c,level,";");return;}
    if(at(c,"{")){emit(c,level,"{");block(c,level+1);emit(c,level,"}");return;}
    if(typeword(now(c)->text)){code=declaration(c);expect(c,";");emit(c,level,"%s;",code);return;}
    if(take(c,"if")){expect(c,"(");e=expr(c,0);numeric(c,e);expect(c,")");emit(c,level,"if (__truth(%s)) {",e.code);block(c,level+1);emit(c,level,"}");
        if(take(c,"else")){if(at(c,"if")){emit(c,level,"else");statement(c,level);}else {emit(c,level,"else {");block(c,level+1);emit(c,level,"}");}}return;}
    if(take(c,"while")){expect(c,"(");e=expr(c,0);numeric(c,e);expect(c,")");emit(c,level,"while (__truth(%s)) {",e.code);c->loop++;block(c,level+1);c->loop--;emit(c,level,"}");return;}
    if(take(c,"for")){int saved=c->ns;char *init,*step;expect(c,"(");c->scope++;init=at(c,";")?"":typeword(now(c)->text)?declaration(c):update(c);expect(c,";");e=at(c,";")?make("1",TINT,now(c)):expr(c,0);numeric(c,e);expect(c,";");step=at(c,")")?"":update(c);expect(c,")");emit(c,level,"for (%s; __truth(%s); %s) {",init,e.code,step);c->loop++;block(c,level+1);c->loop--;c->ns=saved;c->scope--;emit(c,level,"}");return;}
    if(at(c,"break")||at(c,"continue")){Tok *t=now(c);c->pos++;if(!c->loop)fail(c,t,"Semantica","%s solo es valido dentro de un ciclo.",t->text);expect(c,";");emit(c,level,"%s;",t->text);return;}
    if(take(c,"printf")){printf_stmt(c,level);return;}
    if(take(c,"return")){Tok *t=now(c);if(!c->in_main)fail(c,t,"Sintaxis","return solo se admite dentro de main.");e=expr(c,0);numeric(c,e);if(e.type==TDOUBLE)fail(c,t,"Semantica","main debe devolver un entero.");expect(c,";");emit(c,level,"return %s;",convert(c,TINT,e));return;}
    code=update(c);expect(c,";");emit(c,level,"%s;",code);
}
static void statement(Ctx *c,int level){
    if(++c->stmtdepth>128)fail(c,now(c),"Limite","Demasiadas instrucciones anidadas.");
    parse_statement(c,level);c->stmtdepth--;
}
static const char *runtime=
"\"use strict\";\n(() => {\n"
"// Auxiliares de ejecucion para el subconjunto de C documentado.\n"
"const __truth = n => n !== 0;\n"
"const __int = n => { n = Math.trunc(n); if (!Number.isFinite(n) || n < -2147483648 || n > 2147483647) throw new Error('Entero fuera del rango de 32 bits.'); return n; };\n"
"const __char = n => { n = __int(n); if (n < 0 || n > 127) throw new Error('char fuera del rango ASCII (0..127).'); return n; };\n"
"const __div = (a,b) => { if (b === 0) throw new Error('Division entera entre cero.'); return __int(a/b); };\n"
"const __mod = (a,b) => { if (b === 0) throw new Error('Resto entero entre cero.'); return __int(a%b); };\n"
"let __buffer = '';\n"
"const __printf = (format,args) => { let k = 0; __buffer += format.replace(/%(?:\\.(\\d+))?([difs%c])/g, (all,precision,spec) => { if (spec === '%') return '%'; const v = args[k++]; if (spec === 'f') return Number(v).toFixed(precision === undefined ? 6 : Number(precision)); if (spec === 'c') return String.fromCharCode(__char(v)); if (spec === 's') return String(v); return String(__int(v)); }); if (__buffer.length > 50000) throw new Error('Se supero el limite de salida: 50000 caracteres.'); };\n"
"const __main = () => {\n";
int traducir_c_js_con_vista(const char *entrada,char **salida,char **vista,ErrorTraduccion *error){
    Ctx *c;Mem *m,*next;volatile int ok=0;char *volatile owned=NULL;char *volatile body=NULL;
    if(vista)*vista=NULL;
    if(!salida)return 0;
    *salida=NULL;c=(Ctx*)calloc(1,sizeof(Ctx));if(!c){if(error){memset(error,0,sizeof(*error));snprintf(error->mensaje,sizeof(error->mensaje),"Sin memoria.");}return 0;}
    if(setjmp(c->escape)==0){
        size_t start;
        if(!entrada||!entrada[0])fail(c,NULL,"Entrada","Escriba un programa C.");
        if(strlen(entrada)>65536)fail(c,NULL,"Limite","Maximo 65536 bytes por programa.");
        c->out=(char*)alloc(c,MAXOUT);lex(c,entrada);if(now(c)->kind==END)fail(c,now(c),"Entrada","Escriba instrucciones C.");
        emit(c,0,"%s",runtime);
        start=c->len;
        if(at(c,"int")&&c->pos+1<c->nt&&!strcmp(c->toks[c->pos+1].text,"main")){
            expect(c,"int");expect(c,"main");expect(c,"(");take(c,"void");expect(c,")");c->in_main=1;block(c,1);if(now(c)->kind!=END)fail(c,now(c),"Sintaxis","Solo se admite la funcion main.");
        }else while(now(c)->kind!=END)statement(c,1);
        if(vista){
            size_t i=start,n=0;int line_start=1;
            body=(char*)malloc(c->len-start+1);
            if(!body)fail(c,NULL,"Memoria","Sin memoria para la vista.");
            /* Retirar solo la sangria del contenedor, conservando los bloques. */
            while(i<c->len){
                if(line_start){int j=0;while(j<4&&i<c->len&&c->out[i]==' '){i++;j++;}line_start=0;}
                if(i<c->len){char ch=c->out[i++];body[n++]=ch;line_start=ch=='\n';}
            }
            body[n]=0;
        }
        emit(c,0,"};");emit(c,0,"try { __main(); } finally { if (typeof process !== 'undefined' && process.stdout && process.stdout.write) process.stdout.write(__buffer); else if (__buffer) console.log(__buffer); }");
        emit(c,0,"})();");
        owned=(char*)malloc(c->len+1);if(!owned)fail(c,NULL,"Memoria","Sin memoria para la salida.");memcpy(owned,c->out,c->len+1);ok=1;
    }
    if(error)*error=c->error;
    if(!ok){free(body);body=NULL;}
    for(m=c->mem;m;m=next){next=m->next;free(m->ptr);free(m);}free(c);*salida=owned;if(vista)*vista=body;return ok;
}
int traducir_c_js(const char *entrada,char **salida,ErrorTraduccion *error){
    return traducir_c_js_con_vista(entrada,salida,NULL,error);
}
