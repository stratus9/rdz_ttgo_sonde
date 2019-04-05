#include "Scanner.h"
#include <SX1278FSK.h>
#include <U8x8lib.h>

extern U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8;

#define CHANBW 25
#define SMOOTH 2
#define STARTF 400000000
#define NCHAN ((int)(6000/CHANBW))

int scanresult[NCHAN];
int scandisp[NCHAN/2];

#define PLOT_N 120
#define PLOT_MIN -220
#define PLOT_SCALE(x) (x<PLOT_MIN?0:(x-PLOT_MIN)/2)

const byte tilepatterns[9]={0,0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF};
void Scanner::fillTiles(uint8_t *row, int value) {
	for(int y=0; y<8; y++) {
		int nbits = value - 8*(7-y);
		if(nbits<0) { row[8*y]=0; continue; }
		if(nbits>=8) { row[8*y]=255; continue; }
		row[8*y] = tilepatterns[nbits];
	}
}
/*
 * There are 16*8 columns to plot, NPLOT must be lower than that
 * currently, we use 120 * 50kHz channels
 * There are 8*8 values to plot; MIN is bottom end, 
 */
uint8_t tiles[16] = { 0x0f,0x0f,0x0f,0x0f,0xf0,0xf0,0xf0,0xf0, 1, 3, 7, 15, 31, 63, 127, 255};
void Scanner::plotResult()
{
	uint8_t row[8*8];
	for(int i=0; i<PLOT_N; i+=8) {
		for(int j=0; j<8; j++) {
			fillTiles(row+j, PLOT_SCALE(scandisp[i+j]));
		}
		for(int y=0; y<8; y++) {
			u8x8.drawTile(i/8, y, 1, row+8*y);
		}
	}
}

void Scanner::scan()
{
#if 0
	// Test only
	for(int i=0; i<PLOT_N; i++) {
		scandisp[i] = 30*sin(2*3.1415*i/50)-180;
	}
	return;
#endif
	// Configure 
	sx1278.writeRegister(REG_PLL_HOP, 0x80);   // FastHopOn
	sx1278.setRxBandwidth(CHANBW*1000);
	sx1278.writeRegister(REG_RSSI_CONFIG, SMOOTH&0x07);
	sx1278.setFrequency(STARTF);
	sx1278.writeRegister(REG_OP_MODE, FSK_RX_MODE);
	delay(5);

	unsigned long start = millis();
	uint32_t lastfrf=-1;
	for(int i=0; i<NCHAN; i++) {
		float freq = STARTF + 1000.0*i*CHANBW;
		uint32_t frf = freq * 1.0 * (1<<19) / SX127X_CRYSTAL_FREQ;
		if( (lastfrf>>16)!=(frf>>16) ) {
        		sx1278.writeRegister(REG_FRF_MSB, (frf&0xff0000)>>16);
		}
		if( ((lastfrf&0x00ff00)>>8) != ((frf&0x00ff00)>>8) ) {
        		sx1278.writeRegister(REG_FRF_MID, (frf&0x00ff00)>>8);
		}
        	sx1278.writeRegister(REG_FRF_LSB, (frf&0x0000ff));
		lastfrf = frf;
		// Wait TS_HOP (20us) + TS_RSSI ( 2^(SMOOTH+1) / 4 / CHANBW us)
		int wait = 20 + 1000*(1<<(SMOOTH+1))/4/CHANBW;
		delayMicroseconds(wait);
		int rssi = -(int)sx1278.readRegister(REG_RSSI_VALUE_FSK);
		scanresult[i] = rssi;
	}
	unsigned long duration = millis()-start;
	Serial.print("Scan time: ");
	Serial.println(duration);
	for(int i=0; i<NCHAN; i+=2) {
		scandisp[i/2] = scanresult[i];
		for(int j=1; j<2; j++) { if(scanresult[i+j]>scandisp[i/2]) scandisp[i/2] = scanresult[i+j]; }
		Serial.print(scanresult[i]); Serial.print(", ");
		if(((i+1)%32) == 0) Serial.println();
	}
	Serial.println("\n");
}

Scanner scanner = Scanner();