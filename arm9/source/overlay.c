#include "overlay.h"
#include "textstamps.h"

#define MAXP 4

typedef struct {
	int stamp;
	int x, y, scale;
	int r, g, b, alpha;
} Placed;

static Placed layout[MAXP];
static int layoutN = 0;

static u32 oseed = 0x9E3779B9;
static inline u32 orand(void) {
	oseed ^= oseed << 13;
	oseed ^= oseed >> 17;
	oseed ^= oseed << 5;
	return oseed;
}
static inline int rr(int n) { return n > 0 ? (int)(orand() % n) : 0; }

// Mehrfach Weiss, dazu vereinzelt Cyan/Pink/Pastellgelb (wie in den Referenzen).
static const int TINTS[5][3] = {
	{255, 255, 255}, {255, 255, 255}, {185, 240, 255}, {255, 200, 230}, {235, 235, 180}
};
// 8 Randzonen (kein Zentrum). h: 0=links 1=mitte 2=rechts; v: 0=oben 1=mitte 2=unten
static const int ZH[8] = {0, 2, 1, 0, 2, 1, 0, 2};
static const int ZV[8] = {0, 0, 0, 2, 2, 2, 1, 1};

void overlayShuffle(void) {
	int n = 3 + rr(2); // 3 oder 4 Schriften
	if(n > MAXP) n = MAXP;
	layoutN = n;

	// Stamps ohne Wiederholung mischen
	int idx[64];
	int cnt = TEXT_STAMP_COUNT;
	for(int i = 0; i < cnt; i++) idx[i] = i;
	for(int i = cnt - 1; i > 0; i--) { int j = rr(i + 1); int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }

	// Zonen mischen, jede Schrift kriegt eine eigene
	int z[8];
	for(int i = 0; i < 8; i++) z[i] = i;
	for(int i = 7; i > 0; i--) { int j = rr(i + 1); int t = z[i]; z[i] = z[j]; z[j] = t; }

	for(int i = 0; i < n; i++) {
		int s = idx[i];
		int sc = 1;
		if(TEXT_STAMPS[s].w * 2 <= 220 && TEXT_STAMPS[s].h * 2 <= 80 && rr(3) == 0) sc = 2;
		int sw = TEXT_STAMPS[s].w * sc, sh = TEXT_STAMPS[s].h * sc;
		int maxx = 256 - sw; if(maxx < 1) maxx = 1;
		int maxy = 192 - sh; if(maxy < 1) maxy = 1;

		int zone = z[i % 8];
		int hh = ZH[zone], vv = ZV[zone];
		int mx = maxx < 28 ? maxx : 28;
		int my = maxy < 18 ? maxy : 18;
		int x, y;
		if(hh == 0) x = rr(mx + 1);
		else if(hh == 2) x = maxx - rr(mx + 1);
		else x = maxx / 2 + rr(31) - 15;
		if(vv == 0) y = rr(my + 1);
		else if(vv == 2) y = maxy - rr(my + 1);
		else y = maxy / 2 + rr(31) - 15;
		if(x < 0) x = 0; if(x > maxx) x = maxx;
		if(y < 0) y = 0; if(y > maxy) y = maxy;

		int t = rr(5);
		layout[i].stamp = s;
		layout[i].x = x; layout[i].y = y; layout[i].scale = sc;
		layout[i].r = TINTS[t][0]; layout[i].g = TINTS[t][1]; layout[i].b = TINTS[t][2];
		layout[i].alpha = 175 + rr(65); // 175..239
	}
}

void overlayInit(void) {
	overlayShuffle();
}

void overlayDraw(u8 *rgb, int w, int h) {
	for(int i = 0; i < layoutN; i++) {
		const TextStamp *st = &TEXT_STAMPS[layout[i].stamp];
		int sc = layout[i].scale;
		int ox = layout[i].x, oy = layout[i].y;
		int tr = layout[i].r, tg = layout[i].g, tb = layout[i].b, al = layout[i].alpha;
		int dw = st->w * sc, dh = st->h * sc;
		for(int sy = 0; sy < dh; sy++) {
			int py = oy + sy;
			if(py < 0 || py >= h) continue;
			int srow = (sy / sc) * st->w;
			for(int sx = 0; sx < dw; sx++) {
				int px = ox + sx;
				if(px < 0 || px >= w) continue;
				int lum = st->data[srow + sx / sc];
				if(lum < 24) continue;
				int a = (lum * al) / 255;
				int k = (py * w + px) * 3;
				rgb[k]     = (rgb[k] * (255 - a) + tr * a) / 255;
				rgb[k + 1] = (rgb[k + 1] * (255 - a) + tg * a) / 255;
				rgb[k + 2] = (rgb[k + 2] * (255 - a) + tb * a) / 255;
			}
		}
	}
}
