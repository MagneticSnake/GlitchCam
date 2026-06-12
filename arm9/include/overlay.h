#ifndef OVERLAY_H
#define OVERLAY_H

#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Legt ein zufaelliges Schrift-Layout fest.
void overlayInit(void);

// Wuerfelt ein neues Layout (andere Schriften, Positionen, Farben).
void overlayShuffle(void);

// Zeichnet das aktuelle Layout in den RGB888-Puffer (w*h*3).
void overlayDraw(u8 *rgb, int w, int h);

#ifdef __cplusplus
}
#endif

#endif // OVERLAY_H
