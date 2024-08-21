#include <nds.h>
#include <stdio.h>
#include <nds/arm9/console.h>

#include "font.h"
#include "tonccpy.h"
#include "crc32.h"
#include "cardme.h"

#define programSav 0x02000000
#define saveBuff 0x02000200
// #define saveTest 0x02000300

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

const char* textBuffer = "X------------------------------X\nX------------------------------X";

volatile bool UpdateProgressText = false;
volatile bool PrintWithStat = false;
volatile bool ClearOnUpdate = true;
volatile int savetype = 0;
volatile u32 savesize = 0;

extern u8 saveBinary[512];

// DTCM_DATA u8 savebuf[512];
// DTCM_DATA u8 testbuf[256];


bool ReadOrWritePrompt() {
	printf("\n Press [A] to test current save.");
	printf("\n\n Press [B] to write new save\n and exit.\n");
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

int ExitPrompt(bool isError) {
	if (isError) {
		printf("\n Failed to detect a valid\n save chip.\n");
	} else {
		printf("\n Finished save operations.\n");
	}
	printf("\n\n Press [A] to exit.\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		switch (keysDown()) {
			case KEY_A: return 0;
		}
	}
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

int main(void) {
	defaultExceptionHandler();
	CustomConsoleInit();
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
			// cardReadEeprom(0, (u8*)saveBuff, 512, savetype);
			cardmeReadEeprom(0, (u8*)saveBuff, 512, savetype);
			TestSave = (tProgSAVStruct*)saveBuff;
			
			u16 get_crc16 = swiCRC16(0xFFFF, (void*)TestSave->saveBinary, 0x100);
			u32 get_crc32 = crc32((unsigned char*)TestSave->saveBinary, 0x100);

			iprintf(" Expected CRC16: %2X\n", (unsigned int)ProgSave->savCRC16);
			iprintf(" Expected CRC32: %4X\n\n", (unsigned int)ProgSave->savCRC32);
			iprintf(" Got save CRC16: %2X\n", (unsigned int)get_crc16);
			iprintf(" Got save CRC32: %4X\n\n", (unsigned int)get_crc32);
			
			if ((ProgSave->savCRC16 != get_crc16) || (ProgSave->savCRC32 != get_crc32)) {
				if (ProgSave->savCRC16 != get_crc16)printf(" CRC16 mismatch!\n");
				if (ProgSave->savCRC32 != get_crc32)printf(" CRC32 mismatch!\n\n");
				printf(" Battery may need changing!");
			}
		} else {
			if(savetype == 3)cardmeSectorErase(0);
			cardmeWriteEeprom(0, (u8*)programSav, 512, savetype);
		}
	} else {
		return ExitPrompt(true);
	}
	return ExitPrompt(false);
}

