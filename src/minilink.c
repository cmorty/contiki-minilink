/*
 * Copyright (c) 2010, Friedrich-Alexander University Erlangen, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \addtogroup minilink
 * @{
 * \file
 *         Minimalistic linker for msp430.
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 *         Moritz Strübe <Moritz.Struebe@informatik.uni-erlangen.de>
 */

//Start ROM memory at different addresses.
#define DEBUG_DIFF 0


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <io.h>

#include <malloc.h>
#include <cfs/cfs.h>
#include <cfs/cfs-coffee.h>
//#include "mmem.h"
#include <sys/process.h>
#include <dev/watchdog.h>
#include <dev/leds.h>

#include "crc32k.h"
#include "misc_align.h"
#include "minilink.h"

#ifdef BOOTLOADER_VERSION
#include "bootinfo.h"
#endif

#  define Min(a, b)            ( (a)<(b) ? (a) : (b) )       // Take the min between a and b



#define CRCGENBUF_SIZE   64
#define LOADBUF_MIN_SIZE 64

/* Storing data in flash could be faster in blockwriting mode.
 * This requires fixed sized blocks of 64 bytes, otherwise the programming
 * voltage might be applied too long to the same memory area
 * and damage the flash chip. See the MSP430 MCU manual for maximum
 * ratings.
 *
 * DO NOT ENABLE BLOCKWRITING ON REAL HARDWARE UNLESS THE BUFFER
 * MANAGEMENT IS CHANGED TO OUTPUT PROPERLY ALIGNED 64 BYTE BLOCKS!
*/
#define USE_BLOCKWRITING 0

#if USE_BLOCKWRITING
#include "rom.h"
#else
#include <dev/flash.h>
#endif

#define FBENCHMARK 0

#define DEBUG 0
#if DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#define DPUTS(x)     puts(x)
#else
#define DPRINTF(...)
#define DPUTS(x)
#endif


#define DEBUG_LED 0
#if DEBUG_LED
#define LEDRON  leds_red(LEDS_ON)
#define LEDROFF leds_red(LEDS_OFF)
#define LEDGON  leds_green(LEDS_ON)
#define LEDGOFF leds_green(LEDS_OFF)
#define LEDBON  leds_blue(LEDS_ON)
#define LEDBOFF leds_blue(LEDS_OFF)

#else
#define LEDRON
#define LEDROFF
#define LEDGON
#define LEDGOFF
#define LEDBON
#define LEDBOFF
#endif



extern char __noinit_end[];
extern char __stack[];
#ifndef BOOTLOADER_VERSION
extern char __data_end_rom[];
extern char __vectors_start[];
#endif





#define CPY16(dest, src) memcpy(&(dest), &(src), 2)

/*---------------------------------------------------------------------------*/
/** Meta information for checking file integrity */
struct crcgeninfo_st {
  size_t headersize; /**< Size of file header (minimum file size) */
  uint16_t magic;    /**< Magic to identify the file type */
};

struct io_buf_st{
	  uint8_t data[LOADBUF_MIN_SIZE];
	  uint16_t pos;
	  uint16_t filled;
	  int fd;
 };

#if 0
/** Buffered I/O for CFS filesystem */
struct iobuf_st {
  //struct mmem bufdata; /**< Managed Memory containing buffer */
  int fd;              /**< Filedescriptor of underlying cfs file */
  size_t bufsize;      /**< Total size of buffer */
  size_t used;         /**< Number of bytes consumed from the buffer */
  /** Number of valid bytes in the buffer. Will be adjusted automatically
   * when calling shift_iobuf(). Don't modify. */
  size_t avail;
};
#endif

/*---------------------------------------------------------------------------*/
typedef size_t (*MemWriteFunc)(void * dest, void * src, size_t len);
/*---------------------------------------------------------------------------*/
/** Program file meta information */
static const struct crcgeninfo_st
crcgeninfo_pgm = { sizeof(Minilink_Header), MINILINK_PGM_MAGIC };
/** Symbol file meta information */
static const struct crcgeninfo_st
crcgeninfo_sym = { sizeof(Minilink_SymbolHeader), MINILINK_SYM_MAGIC };
/*---------------------------------------------------------------------------*/
/** Location where new software can be installed into flash */
static char *freerom_start;
/** Location where intallable area ends */
static char *freerom_end;


/*---------------------------------------------------------------------------*/




static size_t
memwrite_flash(void *dest, void *src, size_t len)
{
#if USE_BLOCKWRITING
  return rom_pwrite(src, len, (off_t)(uintptr_t)dest);
  // The compiler will remove everything from this point on.
  // -> No #elsif needed
#endif /* USE_BLOCKWRITING */
  size_t written = 0;
  unsigned short *lcldest = dest;
  char *lclsrc = src;
  unsigned short ow;
  char *owptr = (char*)(&ow);

#if DEBUG
  if ((uintptr_t)dest & 1) {
    puts("Alignment FAIL");
  }
#endif
  //DPRINTF("Flash: %x\n", ow);
  //watchdog_periodic();

#if 1 != FBENCHMARK
  flash_setup();
#endif
  while( (len & ~0x1) > written) {
    owptr[0] = *lclsrc++;
    owptr[1] = *lclsrc++;
#if 1 !=FBENCHMARK
    flash_write(lcldest++, ow);
#endif
    written += 2;
  }
#if 1 != FBENCHMARK
  flash_done();
  IFG1 |= UTXIFG0;
#endif
  return written;
}

static void
erasearea_flash(void *start, size_t size)
{
#if USE_BLOCKWRITING
  rom_erase(size, (off_t)(uintptr_t)start);
#endif
  flash_setup();
  while (size > ROM_ERASE_UNIT_SIZE) {
    flash_clear(start);
    size -= ROM_ERASE_UNIT_SIZE;
    start = (char*)start + ROM_ERASE_UNIT_SIZE;
  }
  flash_done();
}

/*---------------------------------------------------------------------------*/
/** Remove consumed bytes from the given I/O buffer.
 *
 * \param b Buffer to operate on.
 */
static void shift_iobuf(struct io_buf_st *b) {
	int status;

#if DEBUG
	if (b->filled < b->pos)
		puts("Buffer underflow!");
#endif

	//Shift data to the beginning
	memmove(b->data, b->data + b->pos, b->filled - b->pos);
	b->filled -= b->pos;
	b->pos = 0;

	status = cfs_read(b->fd, b->data + b->filled, LOADBUF_MIN_SIZE - b->filled);

	b->filled += status;
#if DEBUG
	printf("read: %i", status);
	if (b->filled < LOADBUF_MIN_SIZE)
		puts("EOF encountered.");
#endif
	watchdog_periodic();
}



static void * ml_alloc_text(size_t size){
	if(freerom_start - freerom_end > size){
		freerom_start += size;
		return freerom_start - size;
	}
	return NULL;
}

static void ml_free_mem(void * ptr){
	free(ptr);
}

static void * ml_alloc_mem(size_t size){
	return malloc(size);
}


/*---------------------------------------------------------------------------*/

/** Check program file for consistency.
 *
 * \param filen     File to check
 * \return 1 	File ok
 * \return 0	File not ok
 * \return -2   Problem allocating memory
*/
static int ml_file_check(int myfd, uint16_t magic) {
	int status;
	uint32_t crccmp;
	uint32_t crc_file = 0;
	char first = 0;
	int retval = 0;
	char crcgenbuf[CRCGENBUF_SIZE]; //Put this on stack -> Faster

#if DEBUG
	unsigned checkbytes = 0;
#endif


	//Open the file
	LEDBON;


	if (myfd < 0){
		DPUTS("No FILE\n");
		return retval;
	}
	cfs_seek(myfd, 0, CFS_SEEK_SET);
	LEDBOFF;
	//Initialize CRC varaible
	crc32k_init(&crccmp);

	DPUTS("Checksumming...");
	while(1) {
		status = cfs_read(myfd, crcgenbuf, CRCGENBUF_SIZE);

		if (first == 0) {
			Minilink_CommonHeader *mlch;
			if (status < sizeof(Minilink_CommonHeader) + 2) { //No file is smaller!
				goto cleanup;
			}
			mlch = (Minilink_CommonHeader * )crcgenbuf;

			if(mlch->magic != magic){
				DPRINTF("Magic is %x should be %x\n", mlch->magic, magic);
				goto cleanup;
			}

			crc_file = mlch-> crc; //backup CRC
			mlch->crc = 0; //Set CRC to 0

			first = 1; // Make sure we don't run this again.
		}
		// No data read from File - we're done
		if (status == 0)
			break;

#if DEBUG
		/* DPRINTF("status=%d\n", status); */
		checkbytes += status;
#endif
		crc32k_add(crcgenbuf, status, &crccmp);
	}


	if(crc_file != crccmp){
		DPRINTF("CRC is %08lx should be %08lx \n", crccmp, crc_file);
	} else {
		DPUTS("File ok\n");
		retval = 1;
	}

cleanup:

	//Free mem_hdr & Close file
	DPRINTF("Return: %i\n", retval);
	return retval;
}





/*---------------------------------------------------------------------------*/
/** Read from buffer and perform relocations.
 *
 * \param iob        I/O buffer to read data from
 * \param size       Number of bytes to relocate (destination size!)
 * \param start      Pointer to first output byte, to be passed to mwrite
 * \param mwrite     Memory writing function to use for output
 * \param symvaltab  Table of symbol values
 * \param symcount   Number of symbols in table
 * \return 0 on success, 1 if unexpected EOF or invalid relocation.
*/
static uint_fast8_t ml_relocate(struct io_buf_st *iob, size_t size, uint8_t *start,
		uint16_t *symvaltab, size_t symcount, Minilink_ProgramInfoHeader * pihdr,
		MemWriteFunc mwrite) {

#define OUTBUF_SIZE 16
  uint8_t outbuf[OUTBUF_SIZE];
  size_t outbuf_fill = 0;
  uint16_t escape = 0;
  uint16_t writeaddr = 0;



	//Nothing in the input buffer -> return
	if (iob->filled == 0)
		return 1;


	//Loop through the loaded buffer
	while(size) {

    //We should have at least 3 chars loaded
    if (iob->pos >= iob->filled - 12){

      DPRINTF("Loading new data. Data to go: %i\n", size);
      shift_iobuf(iob);

      //Check whether we got any data in the buffer...
      if (iob->pos >= iob->filled){
        DPUTS("Not enough data");
        return 1;
      }
    }

		//Make sure we have enough buffer
		if (outbuf_fill >= OUTBUF_SIZE - 3){
			size_t written;

			//DPRINTF("W:%x\n", (uint16_t)mwrite);
			written = mwrite(start, outbuf, outbuf_fill);
			if(outbuf_fill - written){
				memmove(outbuf, outbuf + written , outbuf_fill - written);
			}
			start += written;
			outbuf_fill -= written;
			//TODO Remove

		}



		DPRINTF("Offset: %x Char: %x\n",(uint16_t)start + outbuf_fill, iob->data[iob->pos]);

		//Is the current char an escaped char?
		if (iob->data[iob->pos] != MINILINK_RELOC_ESC) {
			//If not write to memory


			if (mwrite == NULL) {
				*(start++) = iob->data[iob->pos];
			} else {
				outbuf[outbuf_fill++] = iob->data[iob->pos];

			}
			iob->pos++;
			size--;
			continue; // Get next char
		}

		//It's an escape - continue
		iob->pos++;

		/* Looks like less then three chars were loaded */
		if (iob->pos >= iob->filled - 3) {
			DPRINTF("Not enaugh Data to handle escape");
		}


		CPY16(escape,  iob->data[iob->pos]);
		iob->pos += 2;

		DPRINTF("Escape: %x\n", escape);

		//This should really be the char.
		if (escape == 0) {
			if (mwrite == NULL) {
				*(start++) = MINILINK_RELOC_ESC;
			} else {
				outbuf[outbuf_fill++] = MINILINK_RELOC_ESC;

			}
			size--;
			continue;
		}
		escape--; //correct offset

		while(1){
			uint8_t mapctr;

			if (escape < symcount) { //A symbol
				writeaddr = symvaltab[escape];
				break;
			}
			//It's not a symbol
			escape -= symcount;

			if (escape < symcount) { //A Symbol with offset
				uint16_t offset;
				CPY16(offset, iob->data[iob->pos]);
				iob->pos += 2;
				writeaddr = symvaltab[escape] + offset;

				break;
			}
			// Looks like it's not a symbol with an offset.
			escape -= symcount;

			for (mapctr = 0; mapctr < MINILINK_SEC; mapctr++) {
			  DPRINTF("CMP %.4x < %.4x \n", escape ,pihdr->mem[mapctr].size);
				if (escape < pihdr->mem[mapctr].size) {
					writeaddr = (uintptr_t)(pihdr->mem[mapctr].ptr) + escape;
					DPRINTF("Setting to  %x = %x + %x\n",writeaddr,(uint16_t)(pihdr->mem[mapctr].ptr), escape);
					break;
				}
				escape -= pihdr->mem[mapctr].size;
			}

			if (mapctr == MINILINK_SEC) {
				DPRINTF("Remaining Reloc %x\n ", escape);
				return 1;
			}
			break;

		}

		DPRINTF("Lnk: %x to %x\n", writeaddr, (uint16_t)start + outbuf_fill );
		if (mwrite == NULL) {
		  CPY16(*start, writeaddr);
			start += 2;
		} else {
			CPY16(outbuf[outbuf_fill], writeaddr);
			outbuf_fill += 2;

		}
		size -= 2;

	}

	// Write remaining data in Output buffer
	if (outbuf_fill){
		mwrite(start, outbuf, outbuf_fill);
		if(outbuf_fill){
			DPUTS("Not all Data written.");
		}
	}

	DPUTS("Relocations OK");
	return 0;
}
/*---------------------------------------------------------------------------*/
#if DEBUG_DIFF == 0
#define INSTPROGRAM_FIRST (ALIGN_ROM_NEXT((uintptr_t)__data_end_rom))
#else
#include "node-id.h"
#define INSTPROGRAM_FIRST (ALIGN_ROM_NEXT((uintptr_t)__data_end_rom) + (node_id) * 30)
#endif
static void
init_freearea_base(void)
{
  freerom_start = (char*)INSTPROGRAM_FIRST;
  freerom_end   = (char*)ALIGN_ROM_PREV((uintptr_t)__vectors_start);
}
/*---------------------------------------------------------------------------*/
/** Determine if given process structure was loaded by minilink.
 * This function assumes, that only minilink adds new processes / protthreds
 * to ram.
 * \param process Pointer to the process structure
 * \return 0 if process is from other source, 1 if process was loaded
 *         by minilink.
 *
*/
int minilink_is_process(struct process *process)
{
  uintptr_t iptr = (uintptr_t)process;
  uintptr_t eptr = (uintptr_t)&__noinit_end;
  if (iptr >= eptr) {
    return 1;
  }
  /* else */
  return 0;
}
/*---------------------------------------------------------------------------*/
/** Remove all programs from flash memory.
 *
 * \return NULL on success. If a linked program is still running,
 *         this function will return a pointer to the process instead.
*/
struct process *
clean_minilink_space(void)
{
  struct process *curproc;

  for (curproc = process_list; curproc != NULL; curproc = curproc->next) {
    if (minilink_is_process(curproc)) return curproc;
  }

  init_freearea_base();
  erasearea_flash(freerom_start, freerom_end - freerom_start);
  return NULL;
}
/*---------------------------------------------------------------------------*/
/** Get next installed program
 * \param Pointer to header of currently selected program, or NULL to get
 *        first program in list.
 * \return Pointer to next entry in list or NULL if last one.
 */
static Minilink_ProgramInfoHeader*
instprog_next(Minilink_ProgramInfoHeader *current) {
	uintptr_t stacktmp;

	if (current == NULL) {
		current = (Minilink_ProgramInfoHeader*) INSTPROGRAM_FIRST;
	} else {
		current = (Minilink_ProgramInfoHeader*) ((char*) current + current->mem[MINILINK_TEXT].size + sizeof(Minilink_ProgramInfoHeader));
	}
	DPRINTF("MAGIC: %x, %x\n", current->magic, MINILINK_INST_MAGIC);
	if (current->magic != MINILINK_INST_MAGIC)
		return NULL;

	stacktmp = (uintptr_t) freerom_end - (uintptr_t) current
			- sizeof(Minilink_ProgramInfoHeader);
	if (current->mem[MINILINK_TEXT].size > (size_t) stacktmp)
		return NULL;

	return current;
}

/**
 * Get the Info-header of a process
 * @param proc The process to get the header of.
 * @return a pointer to the program info header
 */
Minilink_ProgramInfoHeader *
minilink_programm_ih(struct process *proc){
  Minilink_ProgramInfoHeader * pih = NULL;
  struct process  **proclist;
  //puts("pih:");
  while((pih = instprog_next(pih)) != NULL){
    proclist = pih->process;
    //int c;
    //for(c=0; c < 5 ; c++) printf("MEM: %x, %i\n", c, pih->mem[c].size);
    //printf("FL: %s\n", pih->sourcefile);
    //while(proclist != NULL){
      DPRINTF("L:%x F:%x\n", (uint16_t)proc, (uint16_t)(*proclist));

      if((uint16_t)(*proclist) == (uint16_t)(proc)) return pih;
      //proclist++;
    //}

  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/** Check if given program was already linked into rom area.
 *
 * \param proginfo Information structure of the program to find.
 * \return NULL if program not found, or a pointer to the metadata
 *         in case a matching header was found.
 * \todo Check whether the file is installed
 */
static Minilink_ProgramInfoHeader*
program_already_loaded(Minilink_ProgramInfoHeader *proginfo) {
	Minilink_ProgramInfoHeader *instprog = instprog_next(NULL);

	DPUTS("Searching program...");
	for (;;) {
		if (instprog == NULL)
			break;

		if (proginfo->crc == instprog->crc
				&& proginfo->mem[MINILINK_TEXT].size == instprog->mem[MINILINK_TEXT].size
				&& !strncmp(proginfo->sourcefile, instprog->sourcefile, MINILINK_MAX_FILENAME)) {
			DPUTS("Found!");
			return instprog;
		}

		instprog = instprog_next(instprog);
	}

	DPUTS("Not found.");
	return NULL;
}
/*---------------------------------------------------------------------------*/
/** Get the filename from which the given process was loaded from.
 *
 * \param process Pointer to the process structure.
 * \return NULL if program not loaded by minilink, or a string containing
 *         the file name.
*/
const char *
minilink_get_filename(struct process *process)
{
  Minilink_ProgramInfoHeader *instprog = instprog_next(NULL);


  DPUTS("Searching program...");
  for(;;) {
    if (instprog == NULL) break;

    if (   (uintptr_t)(void*)process >= (uintptr_t)instprog->mem[MINILINK_DATA].ptr
        && (uintptr_t)(void*)process <  (uintptr_t)instprog->mem[MINILINK_DATA].ptr + instprog->mem[MINILINK_DATA].size) {
      DPRINTF("Found %s\n", instprog->sourcefile);
      return instprog->sourcefile;
    }

    instprog = instprog_next(instprog);
  }

  DPUTS("Not found.");
  return NULL;
}
/*---------------------------------------------------------------------------*/
/** Initialize minilink internal data.
 * \param stack_space Amount of stack space to reserve.
*/
void
minilink_init(void)
{

  char *tmpptr;

  init_freearea_base();


  DPUTS("Scanning free ROM space...");
  for (tmpptr = freerom_end - 1; tmpptr >= freerom_start; tmpptr--) {
    if (*tmpptr != (char)0xff && *tmpptr != (char)0x00) break;
  }
  freerom_start = (char*)ALIGN_WORD_NEXT((uintptr_t)tmpptr);

  DPUTS("Minilink init OK");

}

/*---------------------------------------------------------------------------*/
/** Link the given file into flash ROM.
 * \param programfile Filename containing program to load
 * \param symtabfile  File containing the symbol table of the kernel
 * \param process     Output for storing pointer to process structure
 *                    of program
 * \return 0 on success, 1 if file was damaged or not found, 2 if not
 *         enough memory, 3 if symbol could not be resolved
 */
uint_fast8_t minilink_load(const char *programfile, const char *symtabfile,
		struct process ***proclist) {
	Minilink_Header mlhdr;
	uint16_t *symvalp = NULL;
	Minilink_ProgramInfoHeader pihdr, *instprog;
	int status = 1;


	struct io_buf_st buf_ml;
	struct io_buf_st buf_sym;

#if DEBUG_DIFF
  void * memblock;
  memblock = malloc(node_id * 40);
#endif


	LEDGOFF;
	LEDBOFF;
  LEDRON;
	memset(&pihdr, 0, sizeof(pihdr));



	if (strlen(programfile) > MINILINK_MAX_FILENAME - 1) {

		DPUTS("Name too long.\n");
		return 1;
	}

	buf_ml.filled = 0;
	buf_ml.pos = 0;

	LEDBON;
	buf_ml.fd = cfs_open(programfile, CFS_READ);

	if (buf_ml.fd < 0) {
		DPUTS("Could not open File.");
		goto cleanup;
	}


	buf_sym.pos = 0;
	buf_sym.filled = 0;

	buf_sym.fd = cfs_open(symtabfile, CFS_READ);
	LEDBOFF;


	//Check whether the files are ok
	if (ml_file_check(buf_ml.fd, MINILINK_PGM_MAGIC) != 1){
		DPUTS("Ret is not 1\n");
		return 1;
	}

	if (ml_file_check(buf_sym.fd , MINILINK_SYM_MAGIC) != 1){
		DPUTS("Ret is not 1\n");
		return 1;
	}
	LEDGON;

	//Reset
	cfs_seek(buf_ml.fd, 0, CFS_SEEK_SET);
	cfs_seek(buf_sym.fd, 0, CFS_SEEK_SET);
	LEDBON;


	//Read header, but do not write to buffer!
	if (cfs_read(buf_ml.fd, &mlhdr, sizeof(mlhdr)) != sizeof(mlhdr)) {
		DPUTS("Could not Read Header.");
		goto cleanup;
	}



	//Now let's get the ram for the symbol table
	symvalp = malloc(mlhdr.symentries * sizeof(uint16_t));
	if (symvalp == NULL) {
		DPUTS("Could not allocate memory for symtbl.");
		status = 2;
		goto cleanup;
	}


	//------------ Resolve the symbol-list. - This must be done anyway
	{
		uint16_t symctr;

		char cursym[MINILINK_MAX_SYMLEN];
		uint16_t curr_add = 0;


		cursym[0] = 0;


		if (buf_sym.fd < 0) {
			DPUTS("Could not open File.");
			goto cleanup;
		}
		{ //get rid of the header.
			uint8_t shift = sizeof(Minilink_SymbolHeader);
			while(shift){
				shift_iobuf(&buf_sym);
				buf_sym.pos = Min(buf_sym.filled, shift);
				shift -= buf_sym.pos;
			}
		}

		//Fill Buffer.....
		shift_iobuf(&buf_sym);

#define NEXTSYMPOS {buf_sym.pos++; if(buf_sym.filled == buf_sym.pos) shift_iobuf(&buf_sym);}


		for (symctr = 0; symctr < mlhdr.symentries; symctr++) {
			uint8_t samechars;

			//fill buffer.
			shift_iobuf(&buf_ml);
			samechars = buf_ml.data[buf_ml.pos];
			buf_ml.pos++;
			DPRINTF("Looking up: <%i>%s\n",samechars, &(buf_ml.data[buf_ml.pos]));

			while (1) {
				/// \fixme make sure we don't go past the buffer
				// The next code is a bit complicated, I'll add some graphix to visualize it
				/// \todo Grafiken erstellen.
				//get next symbol
				uint8_t symattr;
				uint16_t sym_write_pos;

				symattr = buf_sym.data[buf_sym.pos];
				NEXTSYMPOS;

				sym_write_pos = symattr & 0x3F;

				if (samechars > sym_write_pos) { //Ok, looks like we went past the symbol
					DPUTS("Symbol could not be resolved - past same\n");
					status = 1;
					goto cleanup;
				} else if (samechars == sym_write_pos) {
					while (1) { //Loop until we reach the Null-char
						if (buf_sym.data[buf_sym.pos] != buf_ml.data[buf_ml.pos]) break;


						if (buf_ml.data[buf_ml.pos] == '\0'){
							DPUTS("FOUND!\n");
							break; //Could take any of the two, as they are the same
						}
						//It is important that this comes afterwards! - It must point at the NULL
						NEXTSYMPOS;
						buf_ml.pos++;
						samechars++;

					}


					if (buf_sym.data[buf_sym.pos] > buf_ml.data[buf_ml.pos]) { // We are searching for a symbol smaller then
						// the current on. - They are sorted, therefore we will not find it anymore

						DPUTS("Symbol could not be resolved - past alpha\n");
						status = 3;
						goto cleanup;
					}

				}

				while (buf_sym.data[buf_sym.pos] != '\0') NEXTSYMPOS;
				NEXTSYMPOS; //one more!


				symattr &= 0xC0;
				symattr >>= 6;

				switch (symattr) {
					case 0:
					  CPY16(curr_add, buf_sym.data[buf_sym.pos]);
						NEXTSYMPOS;
						break;
					case 1:
						curr_add--;
						curr_add-= buf_sym.data[buf_sym.pos];
						break;
					case 3:
						curr_add += 0x0100;
					case 2:
						curr_add += buf_sym.data[buf_sym.pos];
						break;
				}
				NEXTSYMPOS;

				//DPRINTF("Checking: %i:%s - %x same: %i\n",buf_sym.data[0] & 0x3F , &(buf_sym.data[1]), curr_add, samechars);

				//We've found the symbol, so let's break
				if (buf_ml.data[buf_ml.pos] == '\0'){
					//Move on to next symbol.
					buf_ml.pos ++;
					break;
				}
			} //Loop searching for the symbol

			 ((uint16_t *) (symvalp)) [symctr] = curr_add; //copy the symbol address to memory
			 MALLOC_CHK(symvalp);
		} //Loop looping through symbols
	} // End of resolving symbol list.
#undef NEXTSYMPOS
	LEDGOFF;

	pihdr.magic = MINILINK_INST_MAGIC;
	pihdr.crc = mlhdr.common.crc;
	//pihdr.mem[DATA].ptr = NULL;
	pihdr.mem[MINILINK_DATA].size = mlhdr.datasize;
	//pihdr.mem[MINILINK_BSS].ptr = NULL;
	pihdr.mem[MINILINK_BSS].size = mlhdr.bsssize;
	//pihdr.mem[MINILINK_MIG].ptr = NULL;
	pihdr.mem[MINILINK_MIG].size = mlhdr.migsize;
	//pihdr.mem[MINILINK_MIGPTR].ptr = NULL;
	pihdr.mem[MINILINK_MIGPTR].size = mlhdr.migptrsize;
	//pihdr.process = NULL;
	pihdr.mem[MINILINK_TEXT].size = mlhdr.textsize;
	strncpy(pihdr.sourcefile, programfile, MINILINK_MAX_FILENAME);

	//Let's see whether the program is already installed
	instprog = program_already_loaded(&pihdr);

	if(instprog != NULL){
		struct process * curproc;
		/* Check if program to be reloaded has active processes, i.e. appears
		 * in the process list
		 */
		for (curproc = process_list; curproc != NULL; curproc = curproc->next) {
			if ((uintptr_t) (void*) curproc >= (uintptr_t) (instprog->mem[MINILINK_DATA].ptr)
					&& (uintptr_t) (void*) curproc < (uintptr_t) (instprog->mem[MINILINK_DATA].ptr + instprog->mem[MINILINK_DATA].size)) {
				puts("Process in use. Can't install.");
				status = 2;
				goto cleanup;
			}
		}



		DPRINTF("Loading header from %x\n Data: %x\nBss: %x\n", (uint16_t) instprog, (uint16_t)instprog->mem[MINILINK_DATA].ptr, (uint16_t)instprog->mem[MINILINK_BSS].ptr);

		memcpy(&pihdr, instprog, sizeof(pihdr));
		DPRINTF("After copy:\n Data: %x\nBss: %x\n",  (uint16_t)(pihdr.mem[MINILINK_DATA].ptr), (uint16_t)pihdr.mem[MINILINK_BSS].ptr);
		pihdr.mem[MINILINK_TEXT].ptr = (uint8_t *)instprog + sizeof(pihdr);
	}

	else { //Process does not exist, let's get some memory for linking it
		status = 2;
		uint8_t ctr;
		//Now let's allocate Memory
		if ((mlhdr.textsize & 1) || (mlhdr.datasize & 1) || (mlhdr.bsssize & 1)) {
			DPUTS(".data, .bss or .text section not word aligned");
			goto cleanup;
		}

		pihdr.mem[MINILINK_TEXT].ptr = ml_alloc_text(pihdr.mem[MINILINK_TEXT].size + sizeof(pihdr));
		if (pihdr.mem[MINILINK_TEXT].ptr == NULL) {
			DPUTS("Could not alloc Text.");
			goto cleanup;
		}
		pihdr.mem[MINILINK_TEXT].ptr += sizeof(pihdr);

		//Allocate Memory; Starting beheind text
		for(ctr = MINILINK_DATA; ctr < MINILINK_SEC; ctr ++){
		  if(pihdr.mem[ctr].size){
          pihdr.mem[ctr].ptr = ml_alloc_mem(mlhdr.datasize);
		      if (pihdr.mem[ctr].ptr == NULL) {
		          DPRINTF("Could not alloc Memory for %i\n", ctr);
		          goto cleanup;
		      }
		  }
		}

		pihdr.process = pihdr.mem[MINILINK_TEXT].ptr + mlhdr.processoffset;
		DPRINTF("PO: %.4x = %.4x + %.4x\n", (uintptr_t)pihdr.process, (uintptr_t)pihdr.mem[MINILINK_TEXT].ptr, mlhdr.processoffset);


	}

	// Build up array where to place what....


	{
		uint8_t r;
		for(r = 0; r < MINILINK_SEC; r++){
			DPRINTF("%x len: %x\n", (uintptr_t)(pihdr.mem[r].ptr), (uintptr_t)(pihdr.mem[r].size));
		}
	}
	LEDBOFF;


	// Link data section
	DPRINTF("\n\nRelocating DATA to %x len: %x\n", (uint16_t)pihdr.mem[MINILINK_DATA].ptr, (uint16_t)pihdr.mem[MINILINK_DATA].size);
	status = ml_relocate(&buf_ml, pihdr.mem[MINILINK_DATA].size, pihdr.mem[MINILINK_DATA].ptr, symvalp, mlhdr.symentries, &pihdr, NULL );
	if (status != 0) goto cleanup;
	MALLOC_CHK(symvalp);
  // Link mig section
	if(mlhdr.migsize){
    DPRINTF("\n\nRelocating MIG to %x len: %x\n", (uint16_t)pihdr.mem[MINILINK_MIG].ptr, (uint16_t)pihdr.mem[MINILINK_MIG].size);
    status = ml_relocate(&buf_ml, pihdr.mem[MINILINK_MIG].size, pihdr.mem[MINILINK_MIG].ptr, symvalp, mlhdr.symentries, &pihdr, NULL );
    if (status != 0) goto cleanup;
	}
	MALLOC_CHK(symvalp);
  // Link migptr section
  if(mlhdr.migptrsize){
    DPRINTF("\n\nRelocating MIG to %x len: %x\n", (uint16_t)pihdr.mem[MINILINK_MIGPTR].ptr, (uint16_t)pihdr.mem[MINILINK_MIGPTR].size);
    status = ml_relocate(&buf_ml, pihdr.mem[MINILINK_MIGPTR].size, pihdr.mem[MINILINK_MIGPTR].ptr, symvalp, mlhdr.symentries, &pihdr, NULL );
    if (status != 0) goto cleanup;
  }
  MALLOC_CHK(symvalp);
	//Set Bss to 0
	if(mlhdr.bsssize){
	  DPRINTF("\n\nClearing BSS at %x\n",(uint16_t) pihdr.mem[MINILINK_BSS].ptr);
    memset(pihdr.mem[MINILINK_BSS].ptr, 0, pihdr.mem[MINILINK_BSS].size);
	}

	LEDGON;
	if (instprog == NULL) {
		DPRINTF("\n\nRelocating ROM to %x len: %x\n", (uint16_t) pihdr.mem[MINILINK_TEXT].ptr,(uint16_t) mlhdr.textsize);
		//Buf ML is positioned behind the symbol table
		status = ml_relocate(&buf_ml, mlhdr.textsize, pihdr.mem[MINILINK_TEXT].ptr, symvalp, mlhdr.symentries, &pihdr, &memwrite_flash);
		if (status != 0) goto cleanup;
		MALLOC_CHK(symvalp);

		DPRINTF("\n\nWriting header to %x\n", (uint16_t)pihdr.mem[MINILINK_TEXT].ptr - sizeof(pihdr));
		memwrite_flash(pihdr.mem[MINILINK_TEXT].ptr- sizeof(pihdr),&pihdr, sizeof(pihdr));
		//if(memcmp(pTxt - sizeof(pihdr), &pihdr, sizeof(pihdr)) != 0) printf("\n\nPANIC!!!\n\n");
		{
		  Minilink_ProgramInfoHeader *tmp, *tmp2;
		  tmp2 = &pihdr;
		  tmp = pihdr.mem[MINILINK_TEXT].ptr - sizeof(pihdr);
		  //printf("PIHproc: %x - %x\n", (uint16_t)(tmp->process), (uint16_t)(tmp2->process));
		}

		if (status != 0) goto cleanup;
	}
	LEDROFF;
	LEDGOFF;


	*proclist = pihdr.process;
	DPUTS("Loading complete.");


cleanup:
#if DEBUG_DIFF
  free(memblock);
#endif
	free(symvalp);
	cfs_close(buf_ml.fd);
	cfs_close(buf_sym.fd);
	if (status != 0){
	  uint8_t ctr;
	  //No flash
	  for(ctr = 1; ctr < MINILINK_SEC; ctr++){
	    ml_free_mem(pihdr.mem[ctr].ptr);
	  }
	}

	return status;
}

/** @} */

/*****/
