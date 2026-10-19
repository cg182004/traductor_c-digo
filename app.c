/* Aplicacion nativa de Windows. Compilar app.c + traductor.c.
   Unicode y controles EDIT.
   La aplicacion usa /SUBSYSTEM:WINDOWS: no abre una consola. */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "traductor.h"
#define ID_TRANSLATE 103
#define ID_CLEAR 106
#define ID_SOURCE 201
#define ID_TARGET 202
static HWND source,target,status,leftLabel,rightLabel,buttons[2];
static HFONT fontUI,fontCode,fontTitle,fontSmall;
static HBRUSH background,editorBackground;
static HINSTANCE instance;
static char *generated;
static void notice(HWND owner,const wchar_t *message){MessageBoxW(owner,message,L"Traductor C a JavaScript",MB_OK|MB_ICONINFORMATION);}
static wchar_t *from_utf8(const char *text){
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);wchar_t *w;
    if(!n)return NULL;w=(wchar_t*)calloc((size_t)n,sizeof(wchar_t));if(!w)return NULL;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,w,n)){free(w);return NULL;}return w;
}
static char *to_utf8(const wchar_t *text){
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);char *s;
    if(!n)return NULL;s=(char*)malloc((size_t)n);if(!s)return NULL;
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,s,n,NULL,NULL)){free(s);return NULL;}return s;
}
static wchar_t *read_edit(HWND edit){int n=GetWindowTextLengthW(edit);wchar_t *w=(wchar_t*)calloc((size_t)n+1,sizeof(wchar_t));if(w)GetWindowTextW(edit,w,n+1);return w;}
static void invalidate(void){free(generated);generated=NULL;if(target)SetWindowTextW(target,L"");if(status)SetWindowTextW(status,L"Código modificado.");}
static void translate(HWND owner){
    wchar_t *input=read_edit(source),*out;char *utf,*view=NULL;ErrorTraduccion error;
    invalidate();if(!input){notice(owner,L"No hay memoria suficiente.");return;}utf=to_utf8(input);free(input);
    if(!utf){notice(owner,L"No se pudo leer el texto Unicode.");return;}
    if(traducir_c_js_con_vista(utf,&generated,&view,&error)){
        /* Los controles EDIT de Windows requieren CRLF para varias lineas. */
        size_t i,n=0,len=strlen(view);char *crlf=(char*)malloc(len*2+1);
        if(!crlf){free(view);free(utf);invalidate();notice(owner,L"No hay memoria suficiente.");return;}
        for(i=0;i<len;i++){if(view[i]=='\n')crlf[n++]='\r';crlf[n++]=view[i];}crlf[n]=0;
        free(view);
        out=from_utf8(crlf);free(crlf);
        if(!out){free(utf);invalidate();notice(owner,L"Error al mostrar la traducción.");return;}
        SetWindowTextW(target,out);free(out);
        SetWindowTextW(status,L"Traducción correcta.");
    }else{
        wchar_t message[512];wchar_t *phase=from_utf8(error.fase),*detail=from_utf8(error.mensaje);
        swprintf(message,512,L"%ls · Línea %d, columna %d: %ls",phase?phase:L"Error",error.linea,error.columna,detail?detail:L"Error de traducción.");
        SetWindowTextW(status,message);free(phase);free(detail);
        {LRESULT index=SendMessageW(source,EM_LINEINDEX,(WPARAM)(error.linea-1),0);if(index>=0){index+=error.columna-1;SetFocus(source);SendMessageW(source,EM_SETSEL,(WPARAM)index,(LPARAM)(index+1));SendMessageW(source,EM_SCROLLCARET,0,0);}}
    }free(utf);
}
static HWND control(HWND owner,const wchar_t *cls,const wchar_t *text,DWORD style,int id){
    HWND h=CreateWindowExW(!wcscmp(cls,L"EDIT")?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,10,10,owner,(HMENU)(INT_PTR)id,instance,NULL);SendMessageW(h,WM_SETFONT,(WPARAM)fontUI,TRUE);return h;
}
static void layout(HWND hwnd){
    RECT r;int w,h,gap=18,x=24,y=153,panel,bottom,i;const int widths[2]={120,86};GetClientRect(hwnd,&r);w=r.right;h=r.bottom;panel=(w-48-gap)/2;bottom=h-50;
    for(i=0;i<2;i++){MoveWindow(buttons[i],x,89,widths[i],35,TRUE);x+=widths[i]+8;}
    MoveWindow(leftLabel,24,y-22,panel,20,TRUE);MoveWindow(rightLabel,24+panel+gap,y-22,panel,20,TRUE);
    MoveWindow(source,24,y,panel,bottom-y-12,TRUE);MoveWindow(target,24+panel+gap,y,panel,bottom-y-12,TRUE);
    MoveWindow(status,24,bottom,w-48,35,TRUE);InvalidateRect(hwnd,NULL,TRUE);
}
static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{int i;const wchar_t *labels[2]={L"Traducir →",L"Limpiar"};const int ids[2]={ID_TRANSLATE,ID_CLEAR};
        for(i=0;i<2;i++)buttons[i]=control(hwnd,L"BUTTON",labels[i],WS_TABSTOP|(i==0?BS_DEFPUSHBUTTON:BS_PUSHBUTTON),ids[i]);
        leftLabel=control(hwnd,L"STATIC",L"C  /  CÓDIGO DE ORIGEN",0,0);rightLabel=control(hwnd,L"STATIC",L"JAVASCRIPT  /  RESULTADO",0,0);
        source=control(hwnd,L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL|ES_NOHIDESEL,ID_SOURCE);
        target=control(hwnd,L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,ID_TARGET);
        status=control(hwnd,L"STATIC",L"",SS_LEFT,0);SendMessageW(source,WM_SETFONT,(WPARAM)fontCode,TRUE);SendMessageW(target,WM_SETFONT,(WPARAM)fontCode,TRUE);
        SendMessageW(source,EM_SETLIMITTEXT,65536,0);SendMessageW(target,EM_SETLIMITTEXT,1048576,0);SetWindowTextW(status,L"Listo.");return 0;}
    case WM_SIZE:layout(hwnd);return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize.x=980;((MINMAXINFO*)lp)->ptMinTrackSize.y=600;return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==ID_SOURCE&&HIWORD(wp)==EN_CHANGE){invalidate();return 0;}
        switch(LOWORD(wp)){case ID_TRANSLATE:translate(hwnd);break;
        case ID_CLEAR:SetWindowTextW(source,L"");SetFocus(source);break;}return 0;
    case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;SetTextColor(dc,RGB(62,82,108));SetBkColor(dc,RGB(245,247,251));return (LRESULT)background;}
    case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,RGB(30,45,65));SetBkColor(dc,RGB(255,255,255));return (LRESULT)editorBackground;}
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);FillRect(dc,&r,background);SetBkMode(dc,TRANSPARENT);SelectObject(dc,fontTitle);SetTextColor(dc,RGB(26,84,72));TextOutW(dc,24,18,L"PUENTE  |  C → JavaScript",(int)wcslen(L"PUENTE  |  C → JavaScript"));SelectObject(dc,fontSmall);SetTextColor(dc,RGB(93,111,133));TextOutW(dc,26,57,L"Traductor educativo con interfaz nativa Win32 · Ctrl + Enter para traducir",(int)wcslen(L"Traductor educativo con interfaz nativa Win32 · Ctrl + Enter para traducir"));EndPaint(hwnd,&ps);return 0;}
    case WM_DESTROY:free(generated);generated=NULL;PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE hInstance,HINSTANCE previous,PWSTR command,int show){
    WNDCLASSW wc;HWND hwnd;MSG msg;HACCEL accelerators;int code;ACCEL shortcuts[]={{FVIRTKEY|FCONTROL,VK_RETURN,ID_TRANSLATE}};
    (void)previous;(void)command;instance=hInstance;
    fontUI=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fontCode=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    fontTitle=CreateFontW(-27,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fontSmall=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    background=CreateSolidBrush(RGB(245,247,251));editorBackground=CreateSolidBrush(RGB(255,255,255));ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=WindowProc;wc.hInstance=hInstance;wc.lpszClassName=L"PuenteCJavaScript";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);wc.hbrBackground=background;
    if(!RegisterClassW(&wc))return 1;hwnd=CreateWindowExW(0,wc.lpszClassName,L"Puente · Traductor C a JavaScript",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1160,780,NULL,NULL,hInstance,NULL);if(!hwnd)return 1;
    accelerators=CreateAcceleratorTableW(shortcuts,1);ShowWindow(hwnd,show);UpdateWindow(hwnd);
    while((code=GetMessageW(&msg,NULL,0,0))>0){if(!TranslateAcceleratorW(hwnd,accelerators,&msg)&&!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(accelerators)DestroyAcceleratorTable(accelerators);DeleteObject(fontUI);DeleteObject(fontCode);DeleteObject(fontTitle);DeleteObject(fontSmall);DeleteObject(background);DeleteObject(editorBackground);return code<0?1:(int)msg.wParam;
}
