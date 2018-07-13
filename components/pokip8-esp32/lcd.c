
#include <stdlib.h>
#include <stdio.h>

#include "rom/ets_sys.h"
#include "fb.h"
#include "lcd.h"
#include <string.h>
#include "8bkc-hal.h"
#include "menu.h"

#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"



static uint8_t *frontbuff=NULL, *backbuff=NULL;
static volatile uint8_t *toRender=NULL;
static volatile uint16_t *overlay=NULL;
struct fb fb;
static SemaphoreHandle_t renderSem;

static bool doShutdown=false;

void vid_preinit()
{
}

void gnuboy_esp32_videohandler();

void videoTask(void *pvparameters) {
	gnuboy_esp32_videohandler();
}


void vid_init()
{
	doShutdown=false;
	frontbuff=malloc(64*32);
	backbuff=malloc(64*32);
	
	gbfemtoMenuInit();
	memset(frontbuff, 0, 64 * 32);
	memset(backbuff, 0, 64 * 32);

	renderSem=xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(&videoTask, "videoTask", 1024*2, NULL, 5, NULL, 1);
	ets_printf("Video inited.\n");
}


void vid_close()
{
	doShutdown=true;
	xSemaphoreGive(renderSem);
	vTaskDelay(100); //wait till video thread shuts down... pretty dirty
	free(frontbuff);
	free(backbuff);
	vQueueDelete(renderSem);
}

void vid_settitle(char *title)
{
}

void vid_end()
{
	overlay=NULL;
	toRender=(uint16_t*)fb.ptr;
	xSemaphoreGive(renderSem);
	if (fb.ptr == (unsigned char*)frontbuff ) {
		fb.ptr = (unsigned char*)backbuff;
	} else {
		fb.ptr = (unsigned char*)frontbuff;
	}
//	printf("Pcm %d pch %d\n", patcachemiss, patcachehit);
}

uint32_t *vidGetOverlayBuf() {
	return (uint32_t*)fb.ptr;
}

void vidRenderOverlay() {
	overlay=(uint16_t*)fb.ptr;
	if (fb.ptr == (unsigned char*)frontbuff ) toRender=(uint16_t*)backbuff; else toRender=(uint16_t*)frontbuff;
	xSemaphoreGive(renderSem);
}

void kb_init()
{
}

void kb_close()
{
}

void kb_poll()
{
}

void ev_poll()
{
	kb_poll();
}


uint16_t oledfb[80*64];

int addOverlayPixel(uint16_t p, uint32_t ov) {
	int or, og, ob, a;
	int br, bg, bb;
	int r,g,b;
	br=((p>>11)&0x1f)<<3;
	bg=((p>>5)&0x3f)<<2;
	bb=((p>>0)&0x1f)<<3;

	a=(ov>>24)&0xff;
	//hack: Always show background darker
	a=(a/2)+128;

	ob=(ov>>16)&0xff;
	og=(ov>>8)&0xff;
	or=(ov>>0)&0xff;

	r=(br*(256-a))+(or*a);
	g=(bg*(256-a))+(og*a);
	b=(bb*(256-a))+(ob*a);

	return ((r>>(3+8))<<11)+((g>>(2+8))<<5)+((b>>(3+8))<<0);
}
//This thread runs on core 1.
void gnuboy_esp32_videohandler() {
	int x, y;
	uint16_t *oledfbptr;
	uint16_t c;
	uint32_t *ovl;
	volatile uint8_t *rendering;
	printf("Video thread running\n");
	memset(oledfb, 0, sizeof(oledfb));
	while(!doShutdown) {
		xSemaphoreTake(renderSem, portMAX_DELAY);
		rendering=toRender;
		ovl=(uint32_t*)overlay;
		oledfbptr=oledfb;
		for (y=0; y<64; y++) {
			for (x=0; x<80; x++) {
				// Determine dead zone
				// (It's rendered in the middle of the screen)
				if (y < 16 || y >= 48 || x < 8 || x >= 72)
					c = 0;
				else
					c = rendering[(y - 16) * 48 + (x - 8)] ? 0xFFFF : 0x0;
				if (ovl) c=addOverlayPixel(c, *ovl++);
				*oledfbptr++=(c>>8)+((c&0xff)<<8);
			}
		}
		kchal_send_fb(oledfb);
	}
	vTaskDelete(NULL);
}





