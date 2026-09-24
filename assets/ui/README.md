# Interfaz de la familia dhre

`design-tokens.json` define la paleta, las fuentes y las medidas comunes de
dhreLink y dhreView. Cada repositorio conserva una copia para compilar solo.
Los adaptadores C++ y C# se generan; no se editan manualmente.

```powershell
./scripts/sync-ui-theme.ps1 -PeerRoot ../dhreView
./scripts/sync-ui-theme.ps1 -PeerRoot ../dhreView -Check
```

Desde dhreView, usar `../dhreLink` como PeerRoot. Sin PeerRoot se genera o
comprueba únicamente el proyecto actual. Sincronizar ambos al cambiar el tema.

- Marca: Quintessential regular, 29 px. Controles y datos: Montserrat,
  17 px negrita; etiquetas: 15 px regular; créditos: 14 px regular.
- Colores: violeta para acciones y espera, verde para conexión, ámbar para
  advertencias. Acompañar siempre el color con texto.
- Separación: base 8 px, margen exterior 16 px, separación pequeña 4 px.
  Cabecera compacta de 46 px con icono de 32 px; controles de 40 px.
- Estructura: cabecera, estado/datos de señal, acciones cuando correspondan,
  ruta/ayuda y pie de marca. Las interfaces pueden tener distinta altura.
- Las medidas son píxeles lógicos a 96 DPI. WinForms convierte los tamaños
  tipográficos a puntos; el VST3 usa la escala que solicita el host.
- Los controles personalizados escalan sus detalles de dibujo con el DPI.

OBS usa el mismo adaptador `obs_theme.hpp` en ambos plugins: paneles oscuros,
márgenes, jerarquía y colores explícitos, texto nativo y ajuste de línea.
El marco, botones generales y separación entre propiedades pertenecen a OBS.
QLabel resuelve las familias solicitadas con las fuentes disponibles en su
proceso; si Montserrat o Quintessential no están disponibles, usa su sustitución
nativa. No se instalan fuentes ni se modifica el tema global del host. El VST3
y la aplicación de escritorio integran las mismas fuentes estáticas privadas.
