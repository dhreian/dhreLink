# Arquitectura

## Flujo de audio

```text
DAW
  -> dhreLink Sender (VST3 transparente)
  -> memoria compartida local
  -> dhreLink Receiver (fuente de OBS)
  -> mezclador de OBS
```

No existe una aplicación intermediaria. El VST3 y la fuente de OBS se comunican
directamente mediante el stream fijo `main-mix`; por eso el producto no necesita
una aplicación independiente, puertos de red ni parámetros de conexión.

## Lado del DAW

`dhreLink Sender` acepta mono o estéreo y audio float de 32 o 64 bits. Copia la
entrada a la salida para comportarse como un efecto transparente y publica una
copia float32 después del procesamiento anterior en la cadena.

El plugin reserva el transporte al activarse. Un mutex de Windows impide que dos
emisores escriban el mismo stream al mismo tiempo, por lo que solo debe dejarse
una instancia activa. La publicación dentro del callback no toma locks, no
reserva memoria, no accede a disco y no espera al receptor.

Su editor Win32 es una vista compacta de solo lectura. Muestra los niveles, el
formato y un estado conectado calculado a partir del heartbeat real de OBS. Las
fuentes Quintessential y Montserrat se cargan desde recursos incrustados en el
binario mientras la vista está abierta; no se registran en Windows.

## Transporte

La versión 2 usa un productor y un consumidor. El mapeo de memoria contiene una
cabecera y 128 slots preasignados; cada slot admite hasta 1.024 frames y dos
canales float32. Los buffers mayores se dividen automáticamente.

El productor publica la secuencia solo después de copiar metadatos y muestras.
El consumidor verifica la secuencia antes y después de leer. Si OBS se retrasa
más que la capacidad del anillo, salta al bloque seguro más antiguo disponible
en vez de bloquear el hilo del DAW.

Los heartbeats permiten detectar tanto un emisor cerrado como una fuente de OBS
activa. La cabecera también publica los picos por canal usados por la interfaz.
La fuente de OBS libera el mapeo obsoleto y vuelve a intentar la conexión cada
250 ms. Puede abrirse OBS antes o después del DAW.

## Lado de OBS

Cada fuente `dhreLink Receiver` mantiene un único hilo lector ligero. Entrega audio
planar mono o estéreo a `libobs` con la frecuencia de muestreo informada por el
DAW. OBS se encarga de adaptar esa señal a la configuración de audio del proyecto.

La fuente no abre sockets, no escribe archivos y no conserva preferencias. Al
eliminarla de una escena, detiene y une su hilo antes de liberar el lector.
Su panel de propiedades informa conexión y formato sin ofrecer controles
innecesarios. El módulo entrega a OBS iconos SVG separados para temas claros y
oscuros.

## Instalación

Un solo instalador Inno Setup instala el bundle VST3 completo en la carpeta
global VST3 y el módulo OBS en el layout global de `ProgramData`. El instalador:

- comprueba en compilación que los binarios, iconos y licencias estén presentes;
- guarda sus hashes SHA-256 y verifica los archivos instalados;
- usa Restart Manager durante actualizaciones si un host mantiene un plugin
  cargado;
- desinstala únicamente las dos carpetas pertenecientes a dhreLink.

No instala un monitor, fuentes tipográficas, runtimes de terceros, servicios,
atajos ni tareas de inicio.

## Límites actuales

- Windows x64 y procesos ejecutados en la misma sesión de usuario.
- Un stream mono o estéreo, con un solo emisor activo.
- La fuente usa la API de OBS Studio 32.2 y requiere esa versión o una posterior.
- No hay transporte por red ni sincronización entre computadores.

## Licencias

El core y el VST3 no enlazan con OBS. La fuente OBS es un módulo separado que
enlaza con `libobs` y debe distribuirse bajo términos compatibles con GPL-2.0 o
posterior, junto con la oferta o disponibilidad del código fuente correspondiente.
La distribución pública también debe respetar los términos vigentes del SDK
VST3 de Steinberg. Esta separación técnica no sustituye una revisión legal.
