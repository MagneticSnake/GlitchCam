#ifndef GIF_H
#define GIF_H

#include <nds/ndstypes.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	FILE *f;
	int w, h;
	int ok;
} Gif;

// Animiertes GIF anlegen (loop = 0 fuer Endlosschleife). Gibt 1 bei Erfolg.
int gifStart(Gif *g, const char *path, int w, int h, int loop);

// Einen Frame anhaengen (rgb = w*h*3 Bytes), delayCs = Anzeigedauer in 1/100 s.
int gifFrame(Gif *g, const u8 *rgb, int delayCs);

// GIF abschliessen und Datei schliessen.
int gifEnd(Gif *g);

#ifdef __cplusplus
}
#endif

#endif // GIF_H
