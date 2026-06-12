#ifndef FILTER_H
#define FILTER_H

#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Alle Filter-Parameter, 1:1 wie die Regler im Mac-Filter-Labor.
typedef struct {
	const char *name;
	int pixelate;   // 1..8 Blockgroesse (1 = aus)
	int datamosh;   // 0..100 zufaellige Glitch-Bloecke
	int rgbShift;   // 0..8 Pixel Chroma-Split
	int lift;       // 0..60 Schwarz anheben
	int contrast;   // -60..60
	int threshold;  // 0..100 harter SW-Look (Jabujin)
	int invert;     // 0..100 Negativ
	int sat;        // 0..200 (100 = unveraendert)
	int tintHue;    // 0..360 Farbton fuer den Farbstich
	int tintAmt;    // 0..100 Staerke des Farbstichs
	int posterize;  // 0/2..12 Farbabstufungen (0 = aus)
	int blur;       // 0..3 Weichzeichnen
	int bloom;      // 0..100 Glow auf hellen Stellen
	int edgeGlow;   // 0..100 leuchtende Kanten (Y2K)
	int ghost;      // 0..90 Frame-Blending (nur Vorschau)
	int scan;       // 0..100 Scanlines
	int vignette;   // 0..100
	int grain;      // 0..100 Korn
} FilterParams;

extern const FilterParams FILTERS[];
extern const int FILTER_COUNT;

// Einmalig Scratch-Puffer anlegen (maxPixels = groesste Bildflaeche, z.B. 640*480).
void filterInit(int maxPixels);

// Wendet den Filter auf einen interleavten RGB888-Puffer an (w*h*3 Bytes).
// prevFrame: voriger Frame fuer Ghosting (nur Vorschau), sonst NULL.
void applyFilter(u8 *rgb, int w, int h, const FilterParams *p, u8 *prevFrame);

#ifdef __cplusplus
}
#endif

#endif // FILTER_H
