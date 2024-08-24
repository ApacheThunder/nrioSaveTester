#include <nds.h>
#include <stdio.h>
#include <nds/arm9/console.h>

#include "font.h"
#include "tonccpy.h"
#include "crc32.h"
#include "cardme.h"

#define programSav 0x02000000
#define saveBuff 0x02000200

typedef struct sProgSAVStruct {
	u8 padding[240];
	u32 savCRC32;
	u16 savCRC16;
	u8 padding2[10];
	u8 saveBinary[256];
} tProgSAVStruct;


volatile tProgSAVStruct* ProgSave;
volatile tProgSAVStruct* TestSave;


static PrintConsole tpConsole;
static PrintConsole btConsole;

extern PrintConsole* currentConsole;

static int bg;
static int bgSub;

volatile bool flashText = false;
volatile bool isErrorText = false;
volatile int colorToggle = 0;
volatile int flashTimer = 0;
volatile int savetype = 0;
volatile u32 savesize = 0;

extern u8 saveBinary[512];

bool ReadOrWritePrompt() {
	printf("\n Press \x1b[32;1m[A]\x1b[37;1m to test current save.");
	printf("\n\n Press \x1b[32;1m[B]\x1b[37;1m to write new save\n and exit.\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		switch (keysDown()) {
			case KEY_A: 
				consoleClear();
				while(1) {
					swiWaitForVBlank();
					scanKeys();
					if (keysUp())return true;
				}
				break;
			case KEY_B: return false;
		}
	}
}

int ExitPrompt(bool isError, bool FlashText) {
	if (isError) {
		printf("\n Failed to detect a valid\n save chip.\n");
		printf("\n\n Press \x1b[31;1m[A]\x1b[37;1m to exit.\n");
	} else {
		printf("\n\n\n\n\n Finished test operations.\n");
		printf("\n\n Press \x1b[32;1m[A]\x1b[37;1m to exit.\n");
	}
	flashText = FlashText;
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		switch (keysDown()) {
			case KEY_A: return 0;
		}
	}
	flashText = false;
	return 0;
}

void CustomConsoleInit() {
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA (VRAM_A_MAIN_BG);
	vramSetBankC (VRAM_C_SUB_BG);
	
	bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
		
	consoleInit(&btConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 20, 0, false, false);
	consoleInit(&tpConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 20, 0, true, false);
		
	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 95;
	font.numColors =  fontPalLen / 2;
	font.bpp = 4;
	font.asciiOffset = 32;
	font.convertSingleColor = true;
	consoleSetFont(&btConsole, &font);
	consoleSetFont(&tpConsole, &font);

	consoleSelect(&tpConsole);
}



void vBlankHandler (void) {
	if (flashText) {
		if (flashTimer < 0) {
			flashTimer = 31;
			consoleSelect(&tpConsole);
			consoleClear();
			if (colorToggle == 0) {
				if (isErrorText) {
					printf("\n\n\n\n\n\n\n\n\n\n\n   \x1b[31;1mN-CARD SAVE BATTERY TESTER\x1b[37;1m");
				} else {
					printf("\n\n\n\n\n\n\n\n\n\n\n   \x1b[32;1mN-CARD SAVE BATTERY TESTER\x1b[37;1m");
				}
				colorToggle = 1;
			} else {
				printf("\n\n\n\n\n\n\n\n\n\n\n   N-CARD SAVE BATTERY TESTER");
				colorToggle = 0;
			}
			consoleSelect(&btConsole);
		}
		flashTimer--;
	}
}

int main(void) {
	defaultExceptionHandler();
	CustomConsoleInit();
	irqSet(IRQ_VBLANK, vBlankHandler);
	sysSetCartOwner(true);
	printf("\n\n\n\n\n\n\n\n\n\n\n   N-CARD SAVE BATTERY TESTER");
	consoleSelect(&btConsole);

	savetype = cardmeGetType();
	
	if(savetype > 0)savesize = cardmeSize(savetype);

	if ((savesize > 0) && (savetype > 0)) {
		memset((u8*)saveBuff, 0xFF, 512);
		memset((u8*)programSav, 0xFF, 512);
		tonccpy((u8*)programSav, saveBinary, 512);
		ProgSave = (tProgSAVStruct*)programSav;
		
		if (ReadOrWritePrompt()) {
			consoleClear();
			cardmeReadEeprom(0, (u8*)saveBuff, 512, savetype);
			TestSave = (tProgSAVStruct*)saveBuff;
			
			u16 get_crc16 = swiCRC16(0xFFFF, (void*)TestSave->saveBinary, 0x100);
			u32 get_crc32 = crc32((unsigned char*)TestSave->saveBinary, 0x100);

			iprintf("\n Expected CRC16: %2X", (unsigned int)ProgSave->savCRC16);
			iprintf("\n Expected CRC32: %4X\n\n", (unsigned int)ProgSave->savCRC32);
			
			if ((ProgSave->savCRC16 != get_crc16) || (ProgSave->savCRC32 != get_crc32)) {
				iprintf(" Got save CRC16: \x1b[31;1m%2X\x1b[37;1m\n", (unsigned int)get_crc16);
				iprintf(" Got save CRC32: \x1b[31;1m%4X\x1b[37;1m\n\n", (unsigned int)get_crc32);
			
				if (ProgSave->savCRC16 != get_crc16)printf("\x1b[31;1m CRC16 mismatch!\x1b[37;1m\n");
				if (ProgSave->savCRC32 != get_crc32)printf("\x1b[31;1m CRC32 mismatch!\x1b[37;1m\n\n\n");
				printf(" \x1b[31;1mBattery may need changing!\x1b[37;1m");
				isErrorText = true;
			} else {
				iprintf(" Got save CRC16: \x1b[32;1m%2X\x1b[37;1m\n", (unsigned int)get_crc16);
				iprintf(" Got save CRC32: \x1b[32;1m%4X\x1b[37;1m\n\n\n", (unsigned int)get_crc32);
				printf(" \x1b[32;1mAll CRCs match!\x1b[37;1m");
				isErrorText = false;
			}
		} else {
			consoleClear();
			if(savetype == 3)cardmeSectorErase(0);
			cardmeWriteEeprom(0, (u8*)programSav, 512, savetype);
			printf("\x1b[32;1m\n Save write completed.\x1b[37;1m\n");
			return ExitPrompt(false, false);
		}
	} else {
		isErrorText = true;
		return ExitPrompt(true, true);
	}
	return ExitPrompt(false, true);
}

