/*
; flash.c - flash the ROM in the MSXUSB cartridge
; Copyright (c) 2020 Mario Smit (S0urceror)
; Copyright (c) 2024 Cristiano Goncalves (The Retro Hacker)
; 
; This program writes a KonamiSCC ROM to a flash ROM in the MSXUSB cartridge
; 
; Requirements to compile and use this code:
; - SDCC compiler 3.9 (only!)
; - Fusion-C library 1.3 (also works with 1.2)
; - MSXUSB cartridge with flash ROM
; - KonamiSCC ROM file
;
*/
#include <msx_fusion.h>
#include <io.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "flash.h"
#include "bios.h"

#define SEGMENT_SIZE 8*1024

__at 0x8000 uint8_t file_segment[SEGMENT_SIZE];
__at 0x4000 volatile uint8_t flash_segment[];

static BOOL flash_is_sst = 0;
static uint16_t flash_address1 = 0x555;
static uint16_t flash_address2 = 0x2aa;

void FT_SetName( FCB *p_fcb, const char *p_name )  // Routine servant à vérifier le format du nom de fichier
{
  char i, j;
  memset( p_fcb, 0, sizeof(FCB) );
  for( i = 0; i < 11; i++ ) {
    p_fcb->name[i] = ' ';
  }
  for( i = 0; (i < 8) && (p_name[i] != 0) && (p_name[i] != '.'); i++ ) {
    p_fcb->name[i] =  p_name[i];
  }
  if( p_name[i] == '.' ) {
    i++;
    for( j = 0; (j < 3) && (p_name[i + j] != 0) && (p_name[i + j] != '.'); j++ ) {
      p_fcb->ext[j] =  p_name[i + j] ;
    }
  }
}

/*
    ; main
    ; input: argv, argc
    ; output: none

    ; This program writes a KonamiSCC ROM to a flash ROM in the MSXUSB cartridge
*/
int main(char *argv[], int argc)
{   
    uint8_t slot=0;
    uint8_t argnr=0;
    printf ("MSXUSB Flash Loader 1.2\r\n");
    printf ("(c) 2024 The Retro Hacker\r\n");
    printf ("Based on the original code by S0urceror\r\n\r\n");
    if (argc < 1)
    {
        printf ("FLASH.COM [flags] [romfile]\r\n\r\nOptions:\r\n/S0 - select slot 0\r\n/S1 - select slot 1\r\n/S2 - select slot 2\r\n/S3 - select slot 3\r\n");
        return (0);
    }
    if (ReadSP ()<(0x8000+SEGMENT_SIZE))
    {
        printf ("Not enough memory to read file segment");
        return (0);
    }
    if (strcmp (argv[0],"/S0")==0 || strcmp (argv[0],"/s0")==0) {
        slot = 0;argnr++;
    } 
    if (strcmp (argv[0],"/S1")==0 || strcmp (argv[0],"/s1")==0) {
        slot = 1;argnr++;
    } 
    if (strcmp (argv[0],"/S2")==0 || strcmp (argv[0],"/s2")==0) {
        slot = 2;argnr++;
    } 
    if (strcmp (argv[0],"/S3")==0 || strcmp (argv[0],"/s3")==0) {
        slot = 3;argnr++;
    }

    if (argnr >= argc)
    {
        printf ("Missing ROM filename\r\n");
        return (0);
    }

    if (argnr==0)
    {   
        // find the slot where the flash rom is sitting
        if (!((slot = find_flash())<4))
        {
            printf ("Cannot find slot with flash\r\n");
            return (0);
        } 
    }
    else if (!flash_ident(slot))
    {
        printf ("Cannot find supported flash in slot: %d\r\n",slot);
        return (0);
    }
    printf ("Found flash in slot: %d\r\n",slot);
   
    // file
    FCB fcb;

    FT_SetName (&fcb,argv[argnr]);
    if(fcb_open( &fcb ) != FCB_SUCCESS) 
    {
        printf ("Error: opening file\r\n");
        return (0);   
    }
    printf ("Opened: %s\r\n",argv[0]);

    unsigned long romsize = fcb.file_size;
    printf("Filesize is %ld bytes\r\n", romsize);

    // erase flash sectors
    float endsector = romsize;
    endsector = endsector / 65536;
    endsector = ceilf (endsector);
    if (!erase_flash (slot))
    {
        fcb_close (&fcb);
        return (0); 
    }
    
    // read file from beginning to end and write to flash
    unsigned long total_bytes_written = 0;
    uint8_t segmentnr = 0;
    int bytes_read = 0;

    // while we haven't written the entire file
    while ( total_bytes_written < romsize) 
    {
        // read 8k segment
        MemFill (file_segment,0xff,SEGMENT_SIZE);
        bytes_read = fcb_read( &fcb, file_segment,SEGMENT_SIZE);
        //printf ("Reading %d bytes, segment %d\r\n",bytes_read,segmentnr);

        // check if we read something
        if (bytes_read > 0) {
            // Gauge display (20 chars) - weird behavior with mode 40 that still needs investigation
            int progress = (int)((total_bytes_written + bytes_read) * 20 / romsize); // Scale to 20 chars
            printf("[%-20s] %ld/%ld \r", "####################" + (20 - progress), total_bytes_written + bytes_read, romsize); // write 8k segment (or partial segment)
            
            // write 8k segment (or partial segment)
            if (!write_flash_segment(slot, segmentnr))
                break;

            // update counters
            total_bytes_written += bytes_read;
            segmentnr++;
        }
        else
        {
            printf("Error reading file or end of file reached\r\n");
            break;
        }
        
    }

    // close file
    if (total_bytes_written == romsize)
        printf("\nWrite operation complete!\r\n");
    else
        printf("\nWrite operation failed!\r\n");
    fcb_close (&fcb);
    return(0);
}

/*
    ; select slot 40
    ; input: slot
    ; output: none
*/
void select_slot_40 (uint8_t slot)
{
    slot;
    __asm
    ld  iy,#2
    add iy,sp       
    ld  a,(iy)      
    ld  h,#0x40
    jp	0x24 
    __endasm;
}

/*
    ; select ram slot 40
    ; input: none
    ; output: none
*/
void select_ramslot_40 ()
{
    __asm
    ld	a,(#0xf342) ; RAMAD1
	ld	h,#0x40
	jp	0x24 ; ENASLT
    __endasm;
}

void select_slot_80 (uint8_t slot)
{
    slot;
    __asm
    ld  iy,#2
    add iy,sp
    ld  a,(iy)
    ld  h,#0x80
    jp  0x24
    __endasm;
}

void select_ramslot_80 ()
{
    __asm
    ld  a,(#0xf343) ; RAMAD2
    ld  h,#0x80
    jp  0x24
    __endasm;
}

void delay() {
    for (volatile int i = 0; i < 1000; i++);
}

void hexdump_flash_segment() {
    for (uint16_t i = 0; i < 32768; i += 24) {
        printf("%04X: ", i);
        for (uint16_t j = 0; j < 24; j++) {
            if (i + j < 32768) {
                printf("%02X ", flash_segment[i + j]);
            } else {
                printf("  ");
            }
        }
        printf("\r\n");
    }
}

static void set_flash_type (BOOL sst)
{
    flash_is_sst = sst;
    flash_address1 = sst ? 0x3555 : 0x555;
    flash_address2 = sst ? 0x4aaa : 0x2aa;
}

static void select_flash_commands (uint8_t slot)
{
    if (flash_is_sst)
    {
        // Keep the target bank at 4000h; map SST commands at 7555h and 8AAAh.
        // Mapper writes between unlock cycles would abort the command sequence.
        select_slot_80(slot);
        flash_segment[0x3000] = 2;
        flash_segment[0x5000] = 1;
    }
}

static void restore_flash_commands ()
{
    if (flash_is_sst)
        select_ramslot_80();
}

static void flash_unlock (uint8_t command)
{
    flash_segment[flash_address1] = 0xaa;
    flash_segment[flash_address2] = 0x55;
    flash_segment[flash_address1] = command;
}

/*
    ; flash identification
    ; input: slot
    ; output: TRUE if flash is found, FALSE if not
*/
BOOL flash_ident (uint8_t slot)
{
    uint8_t manufacturer;
    uint8_t device;
    BOOL found = FALSE;

    select_slot_40(slot);
    flash_segment[0] = 0xF0;
    flash_segment[0x1000] = 0;
    set_flash_type(FALSE);
    flash_unlock(0x90);

    manufacturer = flash_segment[0];
    device = flash_segment[1];
    flash_segment[0] = 0xf0;

    switch (device) {
        case 0x86:
            if (manufacturer == 0x37)
            {
                printf("Found device: AMC_A29040B\r\n");
                found = TRUE;
            }
            break;
        case 0xA4:
            if (manufacturer == 0x01)
            {
                printf("Found device: AMD_AM29F040\r\n");
                found = TRUE;
            }
            break;
        case 0x20:
            if (manufacturer == 0x01)
            {
                printf("Found device: AMD_AM29F010\r\n");
                found = TRUE;
            }
            break;
    }

    if (!found)
    {
        if (ReadSP() < 0xc100)
        {
            printf("SST detection requires stack space in page 3\r\n");
            select_ramslot_40();
            return FALSE;
        }
        set_flash_type(TRUE);
        select_flash_commands(slot);
        flash_unlock(0x90);
        manufacturer = flash_segment[0];
        device = flash_segment[1];
        flash_segment[0] = 0xf0;
        restore_flash_commands();
        if (manufacturer == 0xbf && device == 0xb7)
        {
            printf("Found device: SST_SST39SF040\r\n");
            found = TRUE;
        }
        else
            set_flash_type(FALSE);
    }

    select_ramslot_40();
    return found;
}

/*
    ; find flash in slot 40
    ; input: none
    ; output: slot number
*/
uint8_t find_flash ()
{
    uint8_t i;
    uint8_t highest_slot = 4;
    BOOL highest_is_sst = FALSE;
    for (i=0;i<4;i++)
    {
        if (flash_ident (i))
        {
            highest_slot=i; // yes? save slot number
            highest_is_sst = flash_is_sst;
        }
    }
    set_flash_type(highest_is_sst);
    return highest_slot;
}

/*
    ; print hex buffer
    ; input: start, end
    ; output: none
*/
void print_hex_buffer (uint8_t* start, uint8_t* end)
{
    char str[3];
    uint8_t* cur = start;
    uint8_t cnt=0;
    while (cur<end)
    {
        char hex[]="0\0\0";
        uint8_t len = sprintf (str,"%x",*cur);
        if (len<2)
        {
            strcat (hex,str);
            printf (hex);
        }
        else
            printf (str);
        
        cur++;
        cnt++;
        if ((cnt%8)==0)
            printf ("\r\n");
    }
}

/*
    ; erase flash chip
    ; input: slot
    ; output: TRUE if successful, FALSE if not

    ; supports AMD/AMIC and SST command sequences
*/
BOOL erase_flash(uint8_t slot)
{
    // select flash in slot
    select_slot_40 (slot);
    flash_segment[0x1000] = 0;

    printf ("Erasing flash: ");
    select_flash_commands(slot);
    flash_unlock(0x80);
    flash_unlock(0x10);
    restore_flash_commands();

    if (!flash_command_okay (0,0xff))
    {
            // reset
            flash_segment[0] = 0xf0;
            select_ramslot_40();
            printf ("error erasing flash!\r\n");
            return FALSE;
    }

    printf ("done!\r\n");
    select_ramslot_40();
    return TRUE;
}


BOOL flash_command_okay (uint16_t address,uint8_t expected_value)
{
    uint8_t value=0;
    // A bounded poll also handles SST, which has no AMD-style DQ5 timeout bit.
    uint32_t attempts = 1000000UL;
    while (attempts--)
    {
        value = flash_segment[address];
        if ((value & 0x80) == (expected_value & 0x80))
        {
            // SST requires subsequent reads after DQ7 becomes valid.
            value = flash_segment[address];
            if (value == expected_value && flash_segment[address] == expected_value)
                return TRUE;
        }
        if (!flash_is_sst && (value & 0x20) != 0)
            break;
    }
    value = flash_segment[address];
    if (value==expected_value && flash_segment[address]==expected_value)
        return TRUE;
    else
    {
        printf ("=> address: %x, value: %x, response: %x\r\n",address,expected_value,value);
        return FALSE;
    }
}

BOOL write_flash_segment (uint8_t slot,uint8_t segment)
{
    // select flash in slot
    select_slot_40 (slot);
    // select segment
    flash_segment[0x1000] = segment;
    // debug purposes
    // print_hex_buffer (flash_segment, flash_segment+16);
    // write 8k bytes from 0x8000 to 0x4000
    int i;
    for (i=0;i<(8*1024);i++)
    {
        // Page 2 temporarily holds an SST command bank instead of the file buffer.
        uint8_t value = file_segment[i];
        select_flash_commands(slot);
        flash_unlock(0xa0);
        restore_flash_commands();
        flash_segment[i] = value;
        // check if ready
        if (i>=0x1000) // addresses 0x5000 to 0x5fff
            flash_segment[0x1000] = segment; // necessary to switch back
        if (!flash_command_okay (i,value))
        {
            flash_segment[0] = 0xf0;
            printf ("Error writing byte: %x in segment: %d\r\n",i,segment);
            break;   
        }
    }

    // debug purposes
    // print_hex_buffer (flash_segment, flash_segment+16);
    // select ram in slot
    select_ramslot_40 ();

    if (i<(8*1024))
        return FALSE;
    else
        return TRUE;
}