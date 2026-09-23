# dhreLink para macOS: estado y requisitos

**Estado actual:** no existe un instalador macOS funcional. El binario de Windows
no puede incluirse en un `.pkg` para Mac. El producto requiere que funcionen
juntos `dhreLink Sender` (VST3) y `dhreLink Receiver` (modulo de OBS); entregar
solo uno de ellos produciria una instalacion inutilizable.

## Trabajo de portabilidad necesario

1. Validar en macOS el nuevo transporte POSIX de `src/core`: usa `shm_open`,
   `mmap`, un bloqueo exclusivo de emisor y atomicas entre procesos. El test
   automatizado verifica bloques, estado, cierre y reconexion. Aun hace falta
   una prueba de audio de extremo a extremo dentro del DAW y OBS.
2. Crear una vista nativa macOS para el VST3. El editor actual solo acepta
   `kPlatformTypeHWND` y usa GDI/Win32. En Mac, el host entrega una `NSView`.
   Los recursos `.rc` e `.ico` y los enlaces a `user32`/`gdi32` se deben limitar
   a Windows.
3. Compilar el receptor como bundle `.plugin` para OBS macOS, enlazado contra
   una version compatible de `libobs`. La logica de recepcion de audio es en gran
   parte independiente de Windows, pero el CMake actual exige `obs.lib` y
   `obs.dll` de Windows.
4. Probar el flujo completo en un Mac: DAW con VST3 Sender, OBS con Receiver,
   audio mono/estereo, reinicio de cada aplicacion, cierre del emisor y una
   segunda instancia del emisor. Ejecutar el validador VST3 y comprobar ambas
   arquitecturas (`arm64` y `x86_64`).

Cuando esas pruebas pasen, un `.pkg` sin firma Developer ID podra instalar el
VST3 en `/Library/Audio/Plug-Ins/VST3` y el bundle de OBS en la ruta de plugins
que OBS documenta para macOS,
`~/Library/Application Support/obs-studio/plugins`. Los binarios para Apple
Silicon pueden recibir firma local *ad hoc*, sin certificado de pago. Un
workflow manual de GitHub Actions puede generar el paquete dentro de la cuota
gratuita disponible, sin ejecutar builds en cada push.

Este documento no anuncia compatibilidad macOS ni habilita un instalador hasta
que el emisor y el receptor funcionen juntos en un Mac.
