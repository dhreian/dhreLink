# dhreLink para macOS: estado y requisitos

**Estado actual:** el transporte local y las compilaciones universales del
Sender VST3 y el receptor OBS pasan en GitHub Actions. El workflow tambien
genera un `.pkg` sin firma Developer ID y un `.zip`; el artefacto se verifico
por estructura y hashes SHA-256. Aun falta probar audio real entre un DAW y
OBS en Mac. No se anuncia todavia una version funcional.

## Trabajo de portabilidad necesario

1. Validar en macOS el nuevo transporte POSIX de `src/core`: usa `shm_open`,
   `mmap`, un bloqueo exclusivo de emisor y atomicas entre procesos. El test
   automatizado verifica bloques, estado, cierre y reconexion. Aun hace falta
   una prueba de audio de extremo a extremo dentro del DAW y OBS.
2. Probar la nueva vista `NSView` del VST3 Sender dentro de un DAW real. El
   editor macOS muestra conexion y formato de audio, y el build universal
   (`arm64` y `x86_64`) se comprueba en CI. Los recursos `.rc` e `.ico` y los
   enlaces a `user32`/`gdi32` quedan limitados a Windows.
3. Probar el receptor `.plugin` dentro de OBS en macOS. Su build universal usa
   el template oficial de OBS y `libobs` 32.2.2; la prueba de compilacion no
   sustituye comprobar que OBS lo cargue y reciba audio.
4. Probar el flujo completo en un Mac: DAW con VST3 Sender, OBS con Receiver,
   audio mono/estereo, reinicio de cada aplicacion, cierre del emisor y una
   segunda instancia del emisor. Ejecutar el validador VST3 y comprobar ambas
   arquitecturas (`arm64` y `x86_64`).

El workflow de esta rama construye ambos componentes y prepara un `.pkg` sin
firma Developer ID y un `.zip` para instalacion manual. El paquete usa firma
local *ad hoc* gratuita para los bundles. La instalacion es por usuario:
`~/Library/Audio/Plug-Ins/VST3` y
`~/Library/Application Support/obs-studio/plugins`. El repositorio publico usa
runners estandar de GitHub Actions sin cargo de minutos de compilacion.

La publicacion como release debe esperar a que el emisor y el receptor
funcionen juntos en un Mac con un DAW y OBS.
