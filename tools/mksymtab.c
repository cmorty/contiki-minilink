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
 *         Tool to create symbol table from kernel ELF
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 *         Moritz Strübe <Moritz.Struebe@informatik.uni-erlangen.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <bfd.h>
#include <crc32k.h>

#include <inttypes.h>

#include "filelib.h"

#define KERNHEAD_MAXSIZE 128
static unsigned char databuf[KERNHEAD_MAXSIZE];

static inline int LITTLE_ENDIAN(void) {
	int i = 1;
	return  * ( (char *) &i ) ;
}



/**
 * Returns the number of chars which are the same
 */
static int str_num_same(const char * s1, const char * s2){
	int i = 0;
	while(1){
		if(s1[i] != s2[i] || s1[i] == 0 || s2[i] == 0) break;
		i++;
	}
	return i;
}


static int
load_symtab(bfd *source, size_t *symcount, asymbol ***symtab)
{
  long storage_needed, numsyms;
  asymbol **lsymtab;

  storage_needed = bfd_get_symtab_upper_bound(source);
  if (storage_needed < 0) {
    bfd_perror("Unable to retrieve symtab bounds");
    return -1;
  }

  if (storage_needed == 0) {
    *symcount = 0;
    *symtab = NULL;
    return 0;
  }

  lsymtab = malloc(storage_needed);
  if (lsymtab == NULL) {
    perror("Failed to allocate memory for symbol table");
    return -1;
  }

  numsyms = bfd_canonicalize_symtab(source, lsymtab);
  if (numsyms < 0) {
    free(lsymtab);
    bfd_perror("Failed to canonicalize symbol table");
    return -1;
  }

  *symtab = lsymtab;
  *symcount = numsyms;
  return 0;
}

static int
cmp_symname(const void *a, const void *b)
{
  const asymbol *syma = *(asymbol**)a;
  const asymbol *symb = *(asymbol**)b;
  const unsigned char *sa = (const unsigned char *)(syma->name);
  const unsigned char *sb = (const unsigned char *)(symb->name);

  for (;;) {
    if (*sa < *sb) return -1;
    if (*sa == *sb) {
      if (*sa == 0) return 0;
      sa++; sb++;
      continue;
    }
    return 1;
  }
}

static void
sort_symbols_by_name(const size_t symcount, asymbol **symtab)
{
  qsort(symtab, symcount, sizeof(*symtab), cmp_symname);
}

static int
write_symbollist(const size_t symcount, asymbol **symtab, FILE *stream)
{
  size_t slen, i;
  int same_chars;
  unsigned char symlen;
  unsigned char symattrib;
  asymbol *cursym;
  uint16_t symval;
  uint16_t lastsymval = 0;
  uint16_t bytes_written = 0;
  asymbol * lastsym; //The last symbol
  int chars_saved_symbol_name = 0;
  int chars_saved_offset = 0;
  int offset;
  asymbol firstsym;

  //Make sure we can simplyfy the code:
  if(!LITTLE_ENDIAN()){
	  perror("Only works on little endian");
	  return -1;
  }

  //Initialize first pseudo-symbol for the algorithm to work

  firstsym.name = "";
  firstsym.value = 0;
  lastsym = &firstsym;



  for (i = 0; i < symcount; i++) {
    cursym = symtab[i];

    //Let's compress the name:

    slen = strlen(cursym->name) + 1;

    // Find out how many chars are shared with the previous symbol
    same_chars = str_num_same(lastsym->name, cursym->name);

    //No more then 6 Bit!
    if(same_chars > 63) same_chars = 63;

    //some statistics
    chars_saved_symbol_name += same_chars - 1; //The attribute needs a char, too.

    //Chars which are the same must not be written
    slen -= same_chars;


    //Now see if we can save some bytes in the address
    symval = cursym->value + cursym->section->vma;

    // Calculate the offset
    offset = (int)(symval) - (int)lastsymval;

    // Backup the current address
    lastsymval = symval;

    if(offset < -((int)0x100) || offset > 0x1FF){ //Nothing to save :-(
    	symlen = 2;
    	symattrib = 0;
    	//symval does not change
    } else {
    	// Some statistics
    	chars_saved_offset++;
    	symlen = 1;
    	if(offset < 0){
    		symattrib = 1 << 6;
    		symval = (-offset - 1);
    	} else if(offset < 0x100) {
    		// No need to do anything to symval
    		symattrib = 1 << 7;
    		symval = offset;
    	} else {
    		symattrib = (1 << 7) | (1 << 6);
    		symval = offset - 0x100;
    	}
    }



    printf("Same: %i l:%s - attr: %x addr:%x\n", same_chars ,  cursym->name, symattrib, symval);

    // Write attribute an number of same chars
    symattrib |= same_chars;

    bytes_written += 1;
    if( 1 != fwrite(&symattrib, 1, 1, stream)){
        perror("Failed to write symbol attributes");
        return -1;
    }


    //write chars which are not the same
    bytes_written += slen;
    if ( slen != fwrite(&(cursym->name[same_chars]), 1, slen, stream) ) {
      perror("Failed to write symbol name to output");
      return -1;
    }

    //Write address or offset
    bytes_written += symlen;
    if (symlen  != fwrite(&symval, 1, symlen, stream) ) {
      perror("Failed to write symbol value to output");
      return -1;
    }

    lastsym = cursym;
  }

  printf("Total number of symbols: %i\n",symcount);
  printf("Bytes saved by new algo: %i\n", chars_saved_symbol_name);
  printf("Bytes saved using offset: %i of %i \n", chars_saved_offset, symcount);
  printf("Bytes saved in total: %i \n", chars_saved_offset + chars_saved_symbol_name);
  printf("Total size: %i\n", bytes_written );
  printf("Total size without compression: %i\n", bytes_written + chars_saved_offset + chars_saved_symbol_name);

  return 0;
}

static size_t
get_exported_symbols(const size_t symcount, asymbol **symtab,
    asymbol **output)
{
  size_t i, retval = 0;
  asymbol *cursym;

  for (i = 0; i < symcount; i++) {
    cursym = symtab[i];
    if (cursym->flags & BSF_GLOBAL) {
      if (output != NULL) output[retval] = cursym;
      retval++;
    }
  }
  return retval;
}

static int
crc32k_checksum_stream(FILE *stream, uint32_t *checksum) {
  size_t xres;

  crc32k_init(checksum);
  do {
    xres = fread(databuf, 1, sizeof(databuf), stream);
    if (xres < sizeof(databuf) && ferror(stream)) {
	perror("Failed to checksum file");
	return -1;
    }
    crc32k_add(databuf, xres, checksum);
  } while (xres == sizeof(databuf));

  return 0;
}

static int
get_kernel_crc(FILE *infile, uint32_t *out) {
  OSImageInfo kerneldata;
  int status;
  size_t readstatus;

  if (infile == NULL) {
    *out = 0;
    return 0;
  }

  readstatus = fread(databuf, 1, KERNHEAD_MAXSIZE, infile);
  status = read_kernel_header(databuf, readstatus, &kerneldata);
  if (status < 0) {
    if (ferror(infile)) {
      perror("Failed to read from kernel image");
    } else {
      fputs("Kernel image too short.\n", stderr);
    }
    return -1;
  }

  if (kerneldata.signature != MINILINK_KERNEL_SIGNATURE) {
    fputs("Not a kernel file!", stderr);
    return -1;
  }

  *out = kerneldata.imagecrc;
  return 0;
}

static void
print_usage(void) {
  fputs("mksymtab creates a kernel symbol table for linking support\n"
  "Usage:\n"
  "    mksymtab <input> <output> [kernelfile]\n\n"
  "Parameters:\n"
  "    input           ELF File containing kernel\n"
  "    output          Output file to create\n"
  "    kernelfile      Kernel image belonging to ELF input\n\n", stderr);
}

int main(int argc, const char *argv[]) {
  FILE *foutput = NULL, *knlinput = NULL;
  bfd *elfinput = NULL;
  bfd_boolean bfdres;
  int intres, retval = EXIT_FAILURE;
  asymbol **symbol_table = NULL, **sorted_symbol_table = NULL;
  Minilink_SymbolHeader headerdata;
  size_t ffunres, symbol_count, exports_count;

  /* --- check arguments ---------------------------------- */
  if (argc != 3 && argc != 4) {
    fputs("Bad number of arguments.\n\n", stderr);
    print_usage();
    return EXIT_FAILURE;
  }

  /* --- open input files --------------------------------- */
  bfd_init();
  elfinput = bfd_openr(argv[1], NULL);
  if (elfinput == NULL) {
    bfd_perror("Failed to open input file");
    return EXIT_FAILURE;
  }
  bfdres = bfd_check_format(elfinput, bfd_object);
  if (bfdres != TRUE) {
    bfd_perror("Unable to detect input file format");
    fputs("This should be an ELF file containing a compiled kernel\n",
      stderr);
    goto cleanup_closefiles;
  }

  foutput = fopen(argv[2], "w+b");
  if (foutput == NULL) {
    perror("Failed to open output file");
    goto cleanup_closefiles;
  }

  if (argc == 4) {
    knlinput = fopen(argv[3], "r");
    if (knlinput == NULL) {
      perror("Failed to open kernel image");
      goto cleanup_closefiles;
    }
  }

  /* --- load symbol data -------------------------------- */
  intres = load_symtab(elfinput, &symbol_count, &symbol_table);
  if (intres < 0) goto cleanup_closefiles;

  exports_count = get_exported_symbols(symbol_count, symbol_table, NULL);
  sorted_symbol_table = malloc(exports_count *
      sizeof(*sorted_symbol_table));
  if (sorted_symbol_table == NULL) {
    fputs("Not enough memory to sort symbol table.\n", stderr);
    goto cleanup_free;
  }

  get_exported_symbols(symbol_count, symbol_table, sorted_symbol_table);
  sort_symbols_by_name(exports_count, sorted_symbol_table);

  /* --- build header ------------------------------------- */
  headerdata.common.magic = MINILINK_SYM_MAGIC;
  headerdata.common.crc = 0;
  intres = get_kernel_crc(knlinput, &headerdata.kernelchksum);
  if (intres != 0) goto cleanup_free;

  /* --- write output ------------------------------------- */
  intres = convert_symbol_header(&headerdata, databuf, sizeof(databuf));
  if (intres < 0) {
    fputs("Internal error when serializing header data.\n", stderr);
    goto cleanup_free;
  }

  ffunres = fwrite(databuf, 1, intres, foutput);
  if (ffunres != (size_t)intres) {
    perror("Failed to write file header");
    goto cleanup_free;
  }

  intres = write_symbollist(exports_count, sorted_symbol_table, foutput);
  if (intres < 0) goto cleanup_free;

  /* Last byte in file must not be zero, otherwise cfs-coffe won't be able
   * to determine the proper file size.
  */
  intres = putc(0xff, foutput);
  if (intres != 0xff) {
    perror("Failed to write eof marker");
    goto cleanup_free;
  }
  /* --- checksum data ------------------------------------ */
  intres = fseek(foutput, 0, SEEK_SET);
  if (intres != 0) {
    perror("Rewinding output stream failed");
    goto cleanup_free;
  }

  intres = crc32k_checksum_stream(foutput, &headerdata.common.crc);
  if (intres < 0) goto cleanup_free;

  intres = fseek(foutput, 0, SEEK_SET);
  if (intres != 0) {
    perror("Rewinding output stream failed");
    goto cleanup_free;
  }

  intres = convert_symbol_header(&headerdata, databuf, sizeof(databuf));
  if (intres < 0) {
    fputs("Internal error when serializing header data.\n", stderr);
    goto cleanup_free;
  }

  ffunres = fwrite(databuf, 1, intres, foutput);
  if (ffunres != (size_t)intres) {
    perror("Problem overwriting header in output file");
    goto cleanup_free;
  }

  /* --- close/flush files -------------------------------- */
  bfd_close(elfinput);
  elfinput = NULL;

  intres = fclose(foutput);
  if (intres != 0) {
    perror("Failed to close output file");
    foutput = NULL;
    goto cleanup_closefiles;
  }
  foutput = NULL;

  /* --- everything went ok ------------------------------- */

  retval = EXIT_SUCCESS;

cleanup_free:
  free(sorted_symbol_table);
  free(symbol_table);
cleanup_closefiles:
  if (elfinput) bfd_close(elfinput);
  if (foutput)  fclose(foutput);
  if (knlinput) fclose(knlinput);
  return retval;
}
/* @} */
