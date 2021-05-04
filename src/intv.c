/*
	This file is part of FreeIntv.

	FreeIntv is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FreeIntv is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeIntv.  If not, see http://www.gnu.org/licenses/
*/
#include <stdio.h>
#include <stdlib.h>
#include "intv.h"
#include "memory.h"
#include "cp1610.h"
#include "stic.h"
#include "psg.h"
#include "controller.h"
#include "cart.h"
#include "osd.h"

int SR1;

int exec(void);

void LoadGame(const char* path) // load cart rom //
{
	if(LoadCart(path))
	{
		OSD_drawText(3, 3, "LOAD CART: OKAY");
	}
	else
	{
		OSD_drawText(3, 3, "LOAD CART: FAIL");
	}
}

void loadExec(const char* path)
{
	// EXEC lives at 0x1000-0x1FFF
	int i;
	unsigned char word[2];
	FILE *fp;
	if((fp = fopen(path,"rb"))!=NULL)
	{
		for(i=0x1000; i<=0x1FFF; i++)
		{
			fread(word,sizeof(word),1,fp);
			Memory[i] = (word[0]<<8) | word[1];
		}

		fclose(fp);
		OSD_drawText(3, 1, "LOAD EXEC: OKAY");
		printf("[INFO] [FREEINTV] Succeeded loading Executive BIOS from: %s\n", path);		
	}
	else
	{
		OSD_drawText(3, 1, "LOAD EXEC: FAIL");
		printf("[ERROR] [FREEINTV] Failed loading Executive BIOS from: %s\n", path);
	}
}

void loadGrom(const char* path)
{
	// GROM lives at 0x3000-0x37FF
	int i;
	unsigned char word[1];
	FILE *fp;
	if((fp = fopen(path,"rb"))!=NULL)
	{
		for(i=0x3000; i<=0x37FF; i++)
		{
			fread(word,sizeof(word),1,fp);
			Memory[i] = word[0];
		}

		fclose(fp);
		OSD_drawText(3, 2, "LOAD GROM: OKAY");
		printf("[INFO] [FREEINTV] Succeeded loading Graphics BIOS from: %s\n", path);
		
	}
	else
	{
		OSD_drawText(3, 2, "LOAD GROM: FAIL");
		printf("[ERROR] [FREEINTV] Failed loading Graphics BIOS from: %s\n", path);
	}
}

void Reset()
{
	SR1 = 0;
	CP1610Reset();
	MemoryInit();
	STICReset();
	PSGInit();
}

void Init()
{
	CP1610Init();
	MemoryInit();
}

void Run()
{
	// run for one frame 
	// exec will call drawFrame for us only when needed
	while(exec()) { }
}

int exec(void) // Run one instruction 
{
	int ticks = CP1610Tick(0); // Tick CP-1610 CPU, runs one instruction, returns used cycles
	Cycles = Cycles + ticks; 

	if(ticks==0)
	{
		// Halt Instruction found! //
		printf("\n\n[ERROR] [FREEINTV] HALT! at %i\n", Cycles);
		exit(0);
		return 0;
	}

	// Tick PSG
	PSGTick(ticks);
    
    if(SR1>0)
    {
        SR1 = SR1 - ticks;
        if(SR1<0) { SR1 = 0; }
    }
    
    phase_len -= ticks;
    if (phase_len < 0) {
        stic_phase = (stic_phase + 1) & 15;
        switch (stic_phase) {
            case 0:
                stic_reg = 1;
                stic_gram = 1;
                phase_len += 2900;
                SR1 = phase_len;
                if (stic_vid_enable == 1) {
                    // Render Frame //
                    STICDrawFrame();  // Solve this
                    return 0;
                }
                break;
            case 1:
                phase_len += 3796 - 2900;
                stic_vid_enable = DisplayEnabled;
                DisplayEnabled = 0;
                if (stic_vid_enable)
                    stic_reg = 0;
                stic_gram = 1;
                break;
            case 2:
                delayV = ((Memory[0x31])&0x7);
                delayH = ((Memory[0x30])&0x7);
                phase_len += 120 + 114 * delayV + delayH;
                if (stic_vid_enable) {
                    stic_gram = 0;
                    phase_len -= 68;
                    Cycles += 68;
                    PSGTick(68);
                }
                break;
            default:
                phase_len += 912;
                if (stic_vid_enable) {
                    phase_len -= 108;
                    Cycles += 108;
                    PSGTick(108);
                }
                break;
            case 14:
                delayV = ((Memory[0x31])&0x7);
                delayH = ((Memory[0x30])&0x7);
                phase_len += 912 - 114 * delayV - delayH;
                if (stic_vid_enable) {
                    phase_len -= 108;
                    Cycles += 108;
                    PSGTick(108);
                }
                break;
            case 15:
                delayV = ((Memory[0x31])&0x7);
                phase_len += 57 + 17;
                if (stic_vid_enable && delayV == 0) {
                    phase_len -= 38;
                    Cycles += 38;
                    PSGTick(38);
                }
                break;
                
        }
    }
    return 1;
}
