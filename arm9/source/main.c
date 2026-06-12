#include "camera.h"
#include "filter.h"
#include "overlay.h"
#include "subscreen.h"
#include "gif.h"
#include "lodepng.h"
#include "version.h"

#include <dirent.h>
#include <fat.h>
#include <malloc.h>
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PW 256
#define PH 192

int getImageNumber() {
	int highest = -1;
	DIR *pdir = opendir("/DCIM/100DSI00");
	if(pdir == NULL)
		return 0;
	struct dirent *pent;
	while((pent = readdir(pdir)) != NULL) {
		if(strncmp(pent->d_name, "IMG_", 4) == 0) {
			int val = atoi(pent->d_name + 4);
			if(val > highest)
				highest = val;
		}
	}
	closedir(pdir);
	return highest + 1;
}

static int getGifNumber() {
	int highest = -1;
	DIR *pdir = opendir("/DCIM/100DSI00");
	if(pdir == NULL)
		return 0;
	struct dirent *pent;
	while((pent = readdir(pdir)) != NULL) {
		if(strncmp(pent->d_name, "GIF_", 4) == 0) {
			int val = atoi(pent->d_name + 4);
			if(val > highest)
				highest = val;
		}
	}
	closedir(pdir);
	return highest + 1;
}

int main(void) {
	// Oberer Screen: Kamera-Sucher
	vramSetBankA(VRAM_A_MAIN_BG);
	videoSetMode(MODE_5_2D);
	int bg3 = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);
	u16 *gfx = bgGetGfxPtr(bg3);

	// Unterer Screen: Collage-Interface
	subInit();

	bool fatOk = fatInitDefault();
	if(fatOk) {
		mkdir("/DCIM", 0777);
		mkdir("/DCIM/100DSI00", 0777);
	}

	pxiWaitRemote(PXI_CAMERA);
	cameraInit();
	Camera cam = CAM_OUTER;
	cameraActivate(cam);

	u16 *frame = (u16 *)memalign(32, PW * PH * sizeof(u16));
	u8 *rgb = (u8 *)malloc(PW * PH * 3);
	u8 *prev = (u8 *)malloc(PW * PH * 3);
	memset(prev, 0, PW * PH * 3);
	filterInit(PW * PH);
	overlayInit();
	int cur = 0;
	bool havePrev = false;
	bool textOn = true;
	const int GIFN = 20;
	bool recording = false;
	int recFrame = 0;
	u8 *gifbuf = NULL;

	subStatus(FILTERS[cur].name, fatOk ? NULL : "Keine SD");

	while(1) {
		int down = 0;
		swiWaitForVBlank();
		scanKeys();
		down |= keysDown();

		cameraTransferStart(frame, CAPTURE_MODE_PREVIEW);
		while(cameraTransferActive()) {
			swiWaitForVBlank();
			scanKeys();
			down |= keysDown();
		}
		cameraTransferStop();
		DC_InvalidateRange(frame, PW * PH * sizeof(u16));

		// RGB555 -> RGB888
		for(int i = 0; i < PW * PH; i++) {
			u16 px = frame[i];
			int r = px & 31, g = (px >> 5) & 31, b = (px >> 10) & 31;
			rgb[i * 3]     = (r << 3) | (r >> 2);
			rgb[i * 3 + 1] = (g << 3) | (g >> 2);
			rgb[i * 3 + 2] = (b << 3) | (b >> 2);
		}

		applyFilter(rgb, PW, PH, &FILTERS[cur], havePrev ? prev : NULL);
		memcpy(prev, rgb, PW * PH * 3);
		havePrev = true;
		if(textOn)
			overlayDraw(rgb, PW, PH);

		// RGB888 -> RGB555 in den Sucher
		for(int i = 0; i < PW * PH; i++) {
			int r = rgb[i * 3] >> 3, g = rgb[i * 3 + 1] >> 3, b = rgb[i * 3 + 2] >> 3;
			gfx[i] = RGB15(r, g, b) | BIT(15);
		}

		// GIF-Aufnahme
		if(recording) {
			memcpy(gifbuf + recFrame * (PW * PH * 3), rgb, PW * PH * 3);
			recFrame++;
			char m[24];
			siprintf(m, "REC %d/%d", recFrame, GIFN);
			subStatus(FILTERS[cur].name, m);
			if(recFrame >= GIFN) {
				recording = false;
				subStatus(FILTERS[cur].name, "GIF speichern...");
				char gname[40];
				siprintf(gname, "/DCIM/100DSI00/GIF_%04d.GIF", getGifNumber());
				Gif gif;
				if(gifStart(&gif, gname, PW, PH, 0)) {
					for(int fr = 0; fr < GIFN; fr++)
						gifFrame(&gif, gifbuf + fr * (PW * PH * 3), 10);
					gifEnd(&gif);
					subStatus(FILTERS[cur].name, "GIF gespeichert");
				} else {
					subStatus(FILTERS[cur].name, "GIF Fehler");
				}
				free(gifbuf);
				gifbuf = NULL;
			}
		}

		scanKeys();
		down |= keysDown();
		if(down & KEY_R) {
			cur = (cur + 1) % FILTER_COUNT;
			havePrev = false;
			subStatus(FILTERS[cur].name, NULL);
		}
		if(down & KEY_L) {
			cur = (cur + FILTER_COUNT - 1) % FILTER_COUNT;
			havePrev = false;
			subStatus(FILTERS[cur].name, NULL);
		}
		if(down & KEY_Y) {
			cameraDeactivate(cam);
			cam = (cam == CAM_INNER) ? CAM_OUTER : CAM_INNER;
			cameraActivate(cam);
			havePrev = false;
			subStatus(FILTERS[cur].name, cam == CAM_INNER ? "Kamera innen" : "Kamera aussen");
		}
		if(down & KEY_X) {
			textOn = !textOn;
			subStatus(FILTERS[cur].name, textOn ? "Text an" : "Text aus");
		}
		if(down & KEY_B) {
			overlayShuffle();
			subStatus(FILTERS[cur].name, "Schrift gemischt");
		}
		if(fatOk && (down & KEY_SELECT) && !recording) {
			gifbuf = (u8 *)malloc((u32)GIFN * PW * PH * 3);
			if(gifbuf) {
				recording = true;
				recFrame = 0;
				subStatus(FILTERS[cur].name, "GIF Aufnahme...");
			} else {
				subStatus(FILTERS[cur].name, "Zu wenig Speicher");
			}
		}
		if(fatOk && (down & KEY_A)) {
			subStatus(FILTERS[cur].name, "Foto speichern...");
			char name[40];
			siprintf(name, "/DCIM/100DSI00/IMG_%04d.PNG", getImageNumber());
			unsigned err = lodepng_encode24_file(name, rgb, PW, PH);
			subStatus(FILTERS[cur].name, err ? "Speicherfehler" : "Foto gespeichert");
		}
		if(down & KEY_START) {
			cameraDeactivate(cam);
			break;
		}
	}

	free(frame);
	free(rgb);
	free(prev);
	return 0;
}
