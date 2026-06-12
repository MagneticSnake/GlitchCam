#include "filter.h"

#include <nds.h>
#include <stdlib.h>
#include <string.h>

// Vier Ziel-Looks, Werte gespiegelt aus dem Mac-Filter-Labor v2.
// pixelate und sat muessen immer gesetzt sein (sonst 0 = aus bzw. entsaettigt).
const FilterParams FILTERS[] = {
	{.name = "Clean",        .pixelate = 1, .sat = 102, .tintHue = 200, .lift = 6},
	{.name = "Xpiritualism", .pixelate = 1, .sat = 110, .tintHue = 200, .tintAmt = 25, .lift = 20, .contrast = -10, .bloom = 75, .ghost = 60, .blur = 1, .vignette = 20, .grain = 10},
	{.name = "Dreamcore",    .pixelate = 1, .sat = 120, .tintHue = 325, .tintAmt = 24, .lift = 40, .contrast = -22, .bloom = 38, .ghost = 12, .blur = 1, .vignette = 50, .grain = 12},
	{.name = "Jabujin",      .pixelate = 1, .sat = 0,   .tintHue = 200, .threshold = 90, .lift = 42, .contrast = 40, .bloom = 5, .vignette = 12, .grain = 14},
	{.name = "Y2K",          .pixelate = 1, .sat = 115, .tintHue = 188, .tintAmt = 30, .lift = 10, .contrast = 22, .rgbShift = 3, .datamosh = 28, .bloom = 25, .edgeGlow = 55, .scan = 30, .vignette = 18, .grain = 28},
};
const int FILTER_COUNT = sizeof(FILTERS) / sizeof(FILTERS[0]);

// Vorab angelegte Scratch-Puffer (einmal statt pro Bild mallocen = viel schneller auf der DSi).
static u8 *gS1 = NULL, *gS2 = NULL, *gLuma = NULL;

void filterInit(int maxPixels) {
	if(gS1) return;
	gS1 = malloc(maxPixels * 3);
	gS2 = malloc(maxPixels * 3);
	gLuma = malloc(maxPixels);
}

static inline u8 c8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// kleiner xorshift-RNG (Date/Math.random gibt es hier nicht)
static u32 rngState = 0x2545F491;
static inline u32 rnd(void) {
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return rngState;
}

// Farbton (0..360) -> RGB, Saettigung 1, Helligkeit 0.6. Einmal pro Filter, float ok.
static void tintColor(int hue, int *R, int *G, int *B) {
	float hh = hue / 360.0f, l = 0.6f, a = 0.4f;
	int *out[3] = {R, G, B};
	float ns[3] = {0.0f, 8.0f, 4.0f};
	for(int ch = 0; ch < 3; ch++) {
		float k = ns[ch] + hh * 12.0f;
		k = k - 12.0f * (int)(k / 12.0f);
		float m = k - 3.0f;
		float m2 = 9.0f - k;
		if(m2 < m) m = m2;
		if(m > 1.0f) m = 1.0f;
		if(m < -1.0f) m = -1.0f;
		*out[ch] = (int)((l - a * m) * 255.0f);
	}
}

static void pixelate(u8 *d, int w, int h, int b) {
	if(b <= 1) return;
	for(int y = 0; y < h; y += b)
		for(int x = 0; x < w; x += b) {
			int r = 0, g = 0, bl = 0, cnt = 0;
			int ye = y + b > h ? h : y + b, xe = x + b > w ? w : x + b;
			for(int yy = y; yy < ye; yy++)
				for(int xx = x; xx < xe; xx++) {
					int j = (yy * w + xx) * 3;
					r += d[j]; g += d[j + 1]; bl += d[j + 2]; cnt++;
				}
			r /= cnt; g /= cnt; bl /= cnt;
			for(int yy = y; yy < ye; yy++)
				for(int xx = x; xx < xe; xx++) {
					int j = (yy * w + xx) * 3;
					d[j] = r; d[j + 1] = g; d[j + 2] = bl;
				}
		}
}

static void datamosh(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h * 3;
	u8 *c = gS1;
	memcpy(c, d, n);
	int blocks = amt * 14 / 100;
	for(int b = 0; b < blocks; b++) {
		int bw = 16 + (rnd() % 44), bh = 12 + (rnd() % 38);
		if(bw > w) bw = w;
		if(bh > h) bh = h;
		int ox = rnd() % (w - bw + 1), oy = rnd() % (h - bh + 1);
		int sh = 4 + (rnd() % 18), ch = rnd() % 3;
		for(int y = oy; y < oy + bh; y++)
			for(int x = ox; x < ox + bw; x++) {
				int sx = x + sh;
				if(sx > w - 1) sx = w - 1;
				int i = (y * w + x) * 3, s = (y * w + sx) * 3;
				d[i]     = (c[i] * 2 + c[s] * 3) / 5;
				d[i + 1] = (c[i + 1] * 2 + c[s + 1] * 3) / 5;
				d[i + 2] = (c[i + 2] * 2 + c[s + 2] * 3) / 5;
				int v = d[i + ch] + 30;
				d[i + ch] = v > 255 ? 255 : v;
			}
	}
}

static void rgbShift(u8 *d, int w, int h, int a) {
	if(a <= 0) return;
	int n = w * h * 3;
	u8 *c = gS1;
	memcpy(c, d, n);
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			int xr = x - a; if(xr < 0) xr = 0;
			int xb = x + a; if(xb > w - 1) xb = w - 1;
			int i = (y * w + x) * 3;
			d[i]     = c[(y * w + xr) * 3];
			d[i + 2] = c[(y * w + xb) * 3 + 2];
		}
}

static void grade(u8 *d, int w, int h, int lift, int contrast, int sat, int tintHue, int tintAmt) {
	int n = w * h;
	int cf = 256 + contrast * 256 / 100;
	int sf = sat * 256 / 100;
	int tR = 0, tG = 0, tB = 0;
	tintColor(tintHue, &tR, &tG, &tB);
	int ta = tintAmt * 102 / 100;
	for(int i = 0; i < n; i++) {
		int r = d[i * 3], g = d[i * 3 + 1], b = d[i * 3 + 2];
		if(lift) {
			r = lift + r * (255 - lift) / 255;
			g = lift + g * (255 - lift) / 255;
			b = lift + b * (255 - lift) / 255;
		}
		if(contrast) {
			r = ((r - 128) * cf >> 8) + 128;
			g = ((g - 128) * cf >> 8) + 128;
			b = ((b - 128) * cf >> 8) + 128;
		}
		if(sat != 100) {
			int lum = (77 * r + 150 * g + 29 * b) >> 8;
			r = lum + ((r - lum) * sf >> 8);
			g = lum + ((g - lum) * sf >> 8);
			b = lum + ((b - lum) * sf >> 8);
		}
		if(ta) {
			r = (r * (256 - ta) + tR * ta) >> 8;
			g = (g * (256 - ta) + tG * ta) >> 8;
			b = (b * (256 - ta) + tB * ta) >> 8;
		}
		d[i * 3] = c8(r); d[i * 3 + 1] = c8(g); d[i * 3 + 2] = c8(b);
	}
}

static void threshold(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h, m = amt, thr = 100;
	for(int i = 0; i < n; i++) {
		int r = d[i * 3], g = d[i * 3 + 1], b = d[i * 3 + 2];
		int lum = (77 * r + 150 * g + 29 * b) >> 8;
		int bw = lum > thr ? 255 : 0;
		d[i * 3]     = (r * (100 - m) + bw * m) / 100;
		d[i * 3 + 1] = (g * (100 - m) + bw * m) / 100;
		d[i * 3 + 2] = (b * (100 - m) + bw * m) / 100;
	}
}

static void invert(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h * 3, m = amt;
	for(int i = 0; i < n; i++) {
		int v = d[i];
		d[i] = (v * (100 - m) + (255 - v) * m) / 100;
	}
}

static void posterize(u8 *d, int w, int h, int l) {
	if(l < 2) return;
	int n = w * h * 3;
	for(int i = 0; i < n; i++) {
		int v = d[i];
		int q = (v * (l - 1) + 127) / 255;
		d[i] = q * 255 / (l - 1);
	}
}

static void boxPass(u8 *d, int w, int h, int r, int dx, int dy) {
	int n = w * h * 3;
	u8 *c = gS1;
	memcpy(c, d, n);
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			int rr = 0, gg = 0, bb = 0, cnt = 0;
			for(int k = -r; k <= r; k++) {
				int xx = x + dx * k, yy = y + dy * k;
				if(xx < 0 || xx >= w || yy < 0 || yy >= h) continue;
				int j = (yy * w + xx) * 3;
				rr += c[j]; gg += c[j + 1]; bb += c[j + 2]; cnt++;
			}
			int i = (y * w + x) * 3;
			d[i] = rr / cnt; d[i + 1] = gg / cnt; d[i + 2] = bb / cnt;
		}
}

static void blurFx(u8 *d, int w, int h, int r) {
	if(r < 1) return;
	boxPass(d, w, h, r, 1, 0);
	boxPass(d, w, h, r, 0, 1);
}

static void bloom(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h;
	u8 *bp = gS2;
	for(int i = 0; i < n; i++) {
		int r = d[i * 3], g = d[i * 3 + 1], b = d[i * 3 + 2];
		int lum = (77 * r + 150 * g + 29 * b) >> 8;
		int m = lum > 150 ? (lum - 150) * 255 / 105 : 0;
		bp[i * 3] = r * m / 255; bp[i * 3 + 1] = g * m / 255; bp[i * 3 + 2] = b * m / 255;
	}
	boxPass(bp, w, h, 3, 1, 0);
	boxPass(bp, w, h, 3, 0, 1);
	for(int i = 0; i < n * 3; i++) {
		int v = d[i], bv = bp[i] * amt / 100;
		d[i] = c8(255 - (255 - v) * (255 - bv) / 255);
	}
}

static void edgeGlow(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h;
	u8 *L = gLuma;
	for(int i = 0; i < n; i++)
		L[i] = (77 * d[i * 3] + 150 * d[i * 3 + 1] + 29 * d[i * 3 + 2]) >> 8;
	int gR = 90, gG = 220, gB = 255;
	int base = 256 - amt * 256 / 100 * 55 / 100;
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			int xl = x > 0 ? x - 1 : x, xr = x < w - 1 ? x + 1 : x;
			int yu = y > 0 ? y - 1 : y, yd = y < h - 1 ? y + 1 : y;
			int e = abs(L[y * w + xr] - L[y * w + xl]) + abs(L[yd * w + x] - L[yu * w + x]);
			if(e > 255) e = 255;
			int add = e * amt / 100 * 13 / 10;
			int i = (y * w + x) * 3;
			d[i]     = c8((d[i] * base >> 8) + gR * add / 255);
			d[i + 1] = c8((d[i + 1] * base >> 8) + gG * add / 255);
			d[i + 2] = c8((d[i + 2] * base >> 8) + gB * add / 255);
		}
}

static void ghost(u8 *d, u8 *prev, int w, int h, int amt) {
	if(amt <= 0 || !prev) return;
	int n = w * h * 3;
	for(int i = 0; i < n; i++)
		d[i] = (d[i] * (100 - amt) + prev[i] * amt) / 100;
}

static void quant5(u8 *d, int w, int h) {
	int n = w * h * 3;
	for(int i = 0; i < n; i++)
		d[i] = (d[i] & 0xF8) | (d[i] >> 5);
}

static void scanlines(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int f = amt * 60 / 100;
	for(int y = 1; y < h; y += 2)
		for(int x = 0; x < w; x++) {
			int i = (y * w + x) * 3;
			d[i] = d[i] * (100 - f) / 100;
			d[i + 1] = d[i + 1] * (100 - f) / 100;
			d[i + 2] = d[i + 2] * (100 - f) / 100;
		}
}

static void vignetteFx(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int cx = w / 2, cy = h / 2, maxd = cx * cx + cy * cy;
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			int dx = x - cx, dy = y - cy;
			int frac = (dx * dx + dy * dy) * 256 / maxd;
			int fr2 = frac * frac >> 8;
			int atten = amt * 256 / 100 * fr2 >> 8;
			int fac = 256 - atten;
			if(fac < 0) fac = 0;
			int i = (y * w + x) * 3;
			d[i] = d[i] * fac >> 8;
			d[i + 1] = d[i + 1] * fac >> 8;
			d[i + 2] = d[i + 2] * fac >> 8;
		}
}

static void grainFx(u8 *d, int w, int h, int amt) {
	if(amt <= 0) return;
	int n = w * h, g = amt * 45 / 100;
	if(g < 1) return;
	int span = 2 * g + 1;
	for(int i = 0; i < n; i++) {
		int nz = (int)(rnd() % span) - g;
		d[i * 3] = c8(d[i * 3] + nz);
		d[i * 3 + 1] = c8(d[i * 3 + 1] + nz);
		d[i * 3 + 2] = c8(d[i * 3 + 2] + nz);
	}
}

void applyFilter(u8 *rgb, int w, int h, const FilterParams *p, u8 *prevFrame) {
	pixelate(rgb, w, h, p->pixelate);
	datamosh(rgb, w, h, p->datamosh);
	rgbShift(rgb, w, h, p->rgbShift);
	grade(rgb, w, h, p->lift, p->contrast, p->sat, p->tintHue, p->tintAmt);
	threshold(rgb, w, h, p->threshold);
	invert(rgb, w, h, p->invert);
	posterize(rgb, w, h, p->posterize);
	blurFx(rgb, w, h, p->blur);
	bloom(rgb, w, h, p->bloom);
	edgeGlow(rgb, w, h, p->edgeGlow);
	ghost(rgb, prevFrame, w, h, p->ghost);
	quant5(rgb, w, h);
	scanlines(rgb, w, h, p->scan);
	vignetteFx(rgb, w, h, p->vignette);
	grainFx(rgb, w, h, p->grain);
}
