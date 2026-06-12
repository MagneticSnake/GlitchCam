#include "subscreen.h"
#include "subbg.h"
#include "font.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>

#define SW 256
#define SH 192
#define PANEL_TOP 132

#define COL_WHITE (RGB15(31, 31, 31) | BIT(15))
#define COL_CYAN  (RGB15(18, 29, 31) | BIT(15))

static u16 *subGfx = NULL;

static void drawChar(int x, int y, char c, u16 col) {
	int idx = (unsigned char)c - FONT_FIRST;
	if(idx < 0 || idx >= FONT_COUNT) return;
	const unsigned char *g = FONT + idx * FONT_CW * FONT_CH;
	for(int yy = 0; yy < FONT_CH; yy++) {
		int py = y + yy;
		if(py < 0 || py >= SH) continue;
		for(int xx = 0; xx < FONT_CW; xx++) {
			int px = x + xx;
			if(px < 0 || px >= SW) continue;
			if(g[yy * FONT_CW + xx] > 85)
				subGfx[py * SW + px] = col;
		}
	}
}

static void drawText(int x, int y, const char *s, u16 col) {
	for(; *s; s++) {
		drawChar(x, y, *s, col);
		x += FONT_CW;
	}
}

// Panel-Bereich aus dem Originalbild wiederherstellen (loescht alten Text)
static void restorePanel(void) {
	for(int y = PANEL_TOP; y < SH; y++)
		memcpy(subGfx + y * SW, SUBBG + y * SW, SW * sizeof(u16));
}

void subInit(void) {
	videoSetModeSub(MODE_5_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	subGfx = bgGetGfxPtr(bg);
	for(int y = 0; y < SH; y++)
		memcpy(subGfx + y * SW, SUBBG + y * SW, SW * sizeof(u16));
}

void subStatus(const char *filter, const char *msg) {
	if(!subGfx) return;
	restorePanel();
	if(msg && msg[0])
		drawText(5, 135, msg, COL_WHITE);
	else
		drawText(5, 135, "GlitchCam", COL_WHITE);
	char line[40];
	siprintf(line, "Filter: %s", filter);
	drawText(5, 148, line, COL_CYAN);
	drawText(5, 164, "L/R Filter  A Foto  X Text", COL_WHITE);
	drawText(5, 177, "Y Kam  B Mix  SEL GIF  START", COL_WHITE);
}
