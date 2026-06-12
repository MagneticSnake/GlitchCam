#include "gif.h"

#include <stdlib.h>

static void put8(FILE *f, int v) { unsigned char b = (unsigned char)v; fwrite(&b, 1, 1, f); }
static void put16(FILE *f, int v) { put8(f, v & 0xFF); put8(f, (v >> 8) & 0xFF); }

// Feste 256-Farben-Palette: 3 Bit Rot, 3 Bit Gruen, 2 Bit Blau.
static inline int quant(int r, int g, int b) {
	return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}

int gifStart(Gif *g, const char *path, int w, int h, int loop) {
	g->f = fopen(path, "wb");
	g->w = w; g->h = h; g->ok = (g->f != NULL);
	if(!g->ok) return 0;
	FILE *f = g->f;
	fwrite("GIF89a", 1, 6, f);
	put16(f, w); put16(f, h);
	put8(f, 0xF7); // globale Farbtabelle, 256 Eintraege
	put8(f, 0);    // Hintergrundfarbe
	put8(f, 0);    // Seitenverhaeltnis
	for(int i = 0; i < 256; i++) {
		put8(f, ((i >> 5) & 7) * 255 / 7);
		put8(f, ((i >> 2) & 7) * 255 / 7);
		put8(f, (i & 3) * 255 / 3);
	}
	// NETSCAPE-Loop-Extension
	put8(f, 0x21); put8(f, 0xFF); put8(f, 0x0B);
	fwrite("NETSCAPE2.0", 1, 11, f);
	put8(f, 0x03); put8(f, 0x01);
	put16(f, loop);
	put8(f, 0x00);
	return 1;
}

#define HSIZE 8192
static int htab_key[HSIZE];
static int htab_code[HSIZE];

typedef struct { FILE *f; unsigned char buf[256]; int n; u32 acc; int bits; } BitOut;

static void bo_flush(BitOut *b) {
	if(b->n > 0) { b->buf[0] = (unsigned char)b->n; fwrite(b->buf, 1, b->n + 1, b->f); b->n = 0; }
}
static void bo_byte(BitOut *b, unsigned char v) {
	b->buf[1 + b->n] = v; b->n++;
	if(b->n == 255) bo_flush(b);
}
static void bo_code(BitOut *b, int code, int width) {
	b->acc |= ((u32)code) << b->bits; b->bits += width;
	while(b->bits >= 8) { bo_byte(b, (unsigned char)(b->acc & 0xFF)); b->acc >>= 8; b->bits -= 8; }
}
static void bo_finish(BitOut *b) {
	if(b->bits > 0) { bo_byte(b, (unsigned char)(b->acc & 0xFF)); b->acc = 0; b->bits = 0; }
	bo_flush(b);
}

static void lzw(FILE *f, const unsigned char *idx, int npix) {
	BitOut b; b.f = f; b.n = 0; b.acc = 0; b.bits = 0;
	const int clearCode = 256, endCode = 257;
	int width = 9, next = 258;
	for(int i = 0; i < HSIZE; i++) htab_key[i] = -1;
	bo_code(&b, clearCode, width);
	int cur = idx[0];
	for(int i = 1; i < npix; i++) {
		int p = idx[i];
		int key = (cur << 8) | p;
		unsigned hh = ((unsigned)key * 2654435761u) % HSIZE;
		int found = -1;
		while(htab_key[hh] != -1) {
			if(htab_key[hh] == key) { found = htab_code[hh]; break; }
			hh++; if(hh == HSIZE) hh = 0;
		}
		if(found >= 0) {
			cur = found;
		} else {
			bo_code(&b, cur, width);
			// Code-Breite VOR dem Eintragen erhoehen (sonst laeuft der Decoder eine Laenge hinterher)
			if(next >= (1 << width) && width < 12) width++;
			if(next < 4096) {
				htab_key[hh] = key; htab_code[hh] = next; next++;
			} else {
				bo_code(&b, clearCode, width);
				for(int k = 0; k < HSIZE; k++) htab_key[k] = -1;
				width = 9; next = 258;
			}
			cur = p;
		}
	}
	bo_code(&b, cur, width);
	bo_code(&b, endCode, width);
	bo_finish(&b);
}

int gifFrame(Gif *g, const u8 *rgb, int delayCs) {
	if(!g->ok) return 0;
	FILE *f = g->f;
	int w = g->w, h = g->h, npix = w * h;
	// Graphic Control Extension (Verzoegerung)
	put8(f, 0x21); put8(f, 0xF9); put8(f, 0x04);
	put8(f, 0x00);
	put16(f, delayCs);
	put8(f, 0x00);
	put8(f, 0x00);
	// Image Descriptor
	put8(f, 0x2C);
	put16(f, 0); put16(f, 0);
	put16(f, w); put16(f, h);
	put8(f, 0x00);
	// LZW-Bilddaten
	put8(f, 0x08);
	unsigned char *idx = (unsigned char *)malloc(npix);
	if(!idx) return 0;
	for(int i = 0; i < npix; i++)
		idx[i] = (unsigned char)quant(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
	lzw(f, idx, npix);
	free(idx);
	put8(f, 0x00);
	return 1;
}

int gifEnd(Gif *g) {
	if(!g->ok) return 0;
	put8(g->f, 0x3B);
	fclose(g->f);
	g->ok = 0;
	return 1;
}
