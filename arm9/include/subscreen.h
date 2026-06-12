#ifndef SUBSCREEN_H
#define SUBSCREEN_H

#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Unteren Screen aufsetzen: Collage-Hintergrund laden.
void subInit(void);

// Info-Panel neu zeichnen: aktueller Filter plus optionale Statusmeldung (msg = NULL fuer keine).
void subStatus(const char *filter, const char *msg);

#ifdef __cplusplus
}
#endif

#endif // SUBSCREEN_H
