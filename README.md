# dhreLink

dhreLink lleva el audio ya procesado de un DAW a OBS Studio en Windows, sin
crear un dispositivo de audio virtual y sin depender de una aplicación de
monitorización.

La distribución contiene solamente:

- `dhreLink Sender`, un VST3 transparente para el DAW con una interfaz compacta;
- `dhreLink Receiver`, una fuente de audio para OBS Studio con estado visible en
  sus propiedades y un icono propio.

No instala tipografías en Windows, servicios, aplicaciones auxiliares, Node.js
ni procesos en segundo plano. Quintessential y Montserrat están integradas en el
propio VST3 y solo se cargan mientras su interfaz está abierta. El instalador se
muestra simplemente como **dhreLink** y usa la interfaz estándar de Windows.

## Instalar y usar

1. Cierra el DAW y OBS si están abiertos y ejecuta
   `dhreLink-1.0.0-x64-Setup.exe`.
2. En el DAW, añade **dhreLink Sender** al final del master o del bus que quieras
   transmitir. El plugin deja pasar el audio sin modificarlo. Su interfaz muestra
   el formato, los niveles y si una fuente de OBS está recibiendo.
3. En OBS, abre **Fuentes > + > dhreLink Receiver**.
4. Abre las propiedades de esa fuente para ver el estado y el formato recibido.
   Reproduce audio en el DAW: la conexión aparece automáticamente; no hay IP,
   puerto, latencia ni nombre de canal que configurar.

Solo debe existir un `dhreLink Sender` activo. Si el audio también entra a OBS
por Captura de audio del escritorio, desactiva esa segunda ruta para evitar
duplicación o eco.

El instalador coloca los componentes en las ubicaciones globales recomendadas:

```text
C:\Program Files\Common Files\VST3\dhreLink Sender.vst3
C:\ProgramData\obs-studio\plugins\dhreLink\bin\64bit\dhreLink.dll
```

La desinstalación se realiza desde **Configuración de Windows > Aplicaciones** y
solo elimina esas carpetas de dhreLink.

## Compatibilidad actual

- Windows 10/11 x64.
- DAW x64 compatible con VST3 y buses mono o estéreo.
- OBS Studio 32.2 o posterior, x64.
- Una sesión local de Windows; el audio no sale a la red.

## Fiabilidad y latencia

El callback de audio del DAW nunca espera a OBS: escribe en un buffer circular
preasignado de memoria compartida. OBS lee desde su propio hilo y vuelve a
conectarse automáticamente si se abre antes que el DAW o si el DAW se reinicia.
El transporte no ofrece una latencia editable; añade únicamente el tiempo de
los buffers de audio y la planificación normal del sistema.

El build de release ejecuta la prueba del transporte y el Validator oficial de
VST3 antes de crear el instalador. Consulta [la arquitectura](docs/architecture.md)
para los límites técnicos.

## Compilar el instalador

Requisitos de desarrollo:

- Visual Studio 2022 Build Tools con **Desktop development with C++**;
- CMake 3.25 o posterior;
- Git;
- Inno Setup 6;
- OBS Studio 32.2.x instalado.

Desde PowerShell:

```powershell
.\scripts\bootstrap-dependencies.ps1
.\scripts\build-release.ps1
```

El resultado se guarda en:

```text
dist\installer\dhreLink-1.0.0-x64-Setup.exe
```

## Estructura del repositorio

```text
assets/marketing/     Material gráfico para publicación
docs/                 Documentación técnica
packaging/windows/    Definición del instalador de Windows
scripts/              Descarga de dependencias y build reproducible
src/core/             Transporte de audio compartido
src/obs/              Fuente receptora para OBS
src/vst3/             Plugin emisor para el DAW
tests/                Pruebas de transporte, OBS y editor VST3
```

Antes de una distribución pública, los binarios y el instalador deben firmarse
con un certificado de firma de código y probarse en los DAW compatibles que se
quieran anunciar oficialmente.
