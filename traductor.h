#ifndef TRADUCTOR_H
#define TRADUCTOR_H
/* API portable; no depende de Windows. El llamador libera salida con free(). */
typedef struct { int linea, columna; char fase[32], mensaje[256]; } ErrorTraduccion;
int traducir_c_js(const char *entrada, char **salida, ErrorTraduccion *error);
/* Vista del cuerpo sin auxiliares; liberar salida y vista con free(). */
int traducir_c_js_con_vista(const char *entrada, char **salida, char **vista, ErrorTraduccion *error);
#endif
