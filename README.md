
Traductor educativo de un subconjunto de C a JavaScript, escrito en C con interfaz nativa Win32. Realiza análisis léxico, sintáctico y semántico y muestra el cuerpo traducido o el primer error con línea y columna.

## Documentación

- [Informe completo](DOCUMENTACION.md): alcance, gramática, arquitectura, manual de usuario, pruebas y limitaciones.
- [Versión HTML para lectura e impresión](DOCUMENTACION.html): abrir en un navegador; permite guardar el informe como PDF mediante el diálogo de impresión.

## Requisitos y compilación

La interfaz requiere Windows. Para compilar, usar una de estas opciones:

- **MinGW-w64:** agregar GCC al PATH y ejecutar `compilar_mingw.bat`.
- **Visual Studio:** instalar las herramientas de C++ y Windows SDK, abrir la consola de herramientas nativas y ejecutar `compilar_visual_studio.bat`.

Ambas opciones generan `TraductorC_JS.exe` en la carpeta del proyecto. También se puede compilar desde PowerShell si MinGW-w64 está instalado en la siguiente ruta:

```powershell
& "C:\msys64\ucrt64\bin\gcc.exe" -std=c11 -O2 -Wall -Wextra -municode -mwindows app.c traductor.c -o TraductorC_JS.exe -luser32 -lgdi32 -lm
.\TraductorC_JS.exe
```

No se requiere Python ni Node.js para usar la aplicación. Las pruebas automatizadas del motor requieren Python 3, GCC y Node.js; las instrucciones están en el informe.

## Uso

1. Abrir el ejecutable y escribir o pegar C en el editor izquierdo.
2. Pulsar **Traducir** o **Ctrl + Enter**.
3. Revisar el resultado o el error de la barra inferior.
4. Pulsar **Limpiar** para comenzar de nuevo.

```c
int main(void) {
    int suma = 0;
    for (int i = 1; i <= 5; i++) {
        suma += i;
    }
    printf("Suma: %d\n", suma);
    return 0;
}
```

La vista muestra el cuerpo de la traducción y puede copiarse con Ctrl + C. Contiene llamadas a auxiliares como `__int` y `__printf`: para ejecutarlo se necesita el programa completo que devuelve la API del motor. La interfaz actual dispone de **Traducir** y **Limpiar**; no incluye exportación ni ejecución de JavaScript.

## Contenido

| Ruta | Descripción |
| --- | --- |
| `app.c` | Interfaz gráfica Win32 |
| `traductor.c`, `traductor.h` | Motor portable y API pública |
| `compilar_*.bat` | Compilación en Windows |
| `ejemplos/` | Entradas C, ejemplos JS y resultados esperados |
| `tests/` | Pruebas del motor y de la vista |
| `DOCUMENTACION.md`, `DOCUMENTACION.html` | Informe técnico y manual |

Se admiten variables inicializadas `int`, `double` y `char` ASCII, condiciones, ciclos, `printf` y algunas funciones matemáticas. Los bloques de control requieren llaves. El proyecto implementa un subconjunto académico de C; el informe detalla sus restricciones.

## Entrega

Completar los datos de la portada del informe, compilar y comprobar la aplicación en Windows. Entregar fuentes, documentación, ejemplos y scripts; añadir el ejecutable recién compilado si la actividad lo requiere. El HTML permite preparar un PDF para adjuntarlo a la entrega.
