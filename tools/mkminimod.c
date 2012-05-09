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
 *         Tool to create loadable program
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 *         Moritz Str�be <Moritz.Struebe@informatik.uni-erlangen.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <bfd.h>
#include <crc32k.h>
#include <limits.h>
#include <stdint.h>
#include "minilink.h"
#include "filelib.h"


#define FILEHEAD_MAXSIZE 128
#define PROCESS_ENTRY_NAME "autostart_processes"
#define RELTYPE_01_1 "R_MSP430_16"
#define RELTYPE_01_2 "R_MSP430_16_BYTE"

#define SFWRITE(data, size, stream) {   \
    if (size != fwrite(data, 1, size, stream)) { \
      perror("Failed writing file: " /*__FILE__ " " __LINE__*/); \
      return -1; \
    } \
  }


static unsigned char databuf[FILEHEAD_MAXSIZE];

typedef unsigned char BitArray;
#define bitarray_calc_idx(bit) ((bit) / CHAR_BIT)
#define bitarray_calc_bit(bit) (1u << ((bit) % CHAR_BIT))
#define bitarray_get(array, bit) \
    ((array)[bitarray_calc_idx(bit)] & bitarray_calc_bit(bit))
#define bitarray_set(array, bit) \
    ((array)[bitarray_calc_idx(bit)] |= bitarray_calc_bit(bit))


struct s_stats{
  int esc;
  int link_simp;
  int link_comp;
  int reloc;
}lstats, tstats;

#define NUMSECT 5

static struct
{
  char name[16]; //The name of the section
  asection *sectptr;
  arelent **reloc;
  arelent ***sorted_reloc;
  size_t reloc_count;
  size_t size;
  bfd_byte *content;
  unsigned required:1; // Is the section required
  unsigned has_relocations; //does the section contain relocations
  unsigned available:1; // Is the section required - DO NOT SET
} sections[NUMSECT] =
{
{ .name = ".text", .required =1, .has_relocations = 1 },
{ .name = ".data", .required =1, .has_relocations = 1 },
{ .name = ".bss", .required =1, .has_relocations = 0 },
{ .name = "mig", .required = 0, .has_relocations = 1 },
{ .name = "mig_ptr", .required = 0, .has_relocations = 1  }
};

static int
load_symtab(bfd *source, size_t *symcount, asymbol ***symtab)
{
  long storage_needed, numsyms;
  asymbol **lsymtab;  ///< Local symbol table

  // Find out how much storage is needed to save the symbols
  // We get the syze in bytes
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

  //Fill lsymtab with pointers to the symbol table
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
load_relocations(asection *sect_src, asymbol **symtab, size_t *relcount,
    arelent ***rels)
{
  long storage_needed, numrels;
  arelent **lrels;

  storage_needed = bfd_get_reloc_upper_bound(sect_src->owner, sect_src);
  if (storage_needed < 0) {
    bfd_perror("Unable to determine storage requirements for relocation");
    return -1;
  }

  if (storage_needed == 0) {
    *symtab = NULL;
    *relcount = 0;
    return 0;
  }

  lrels = malloc(storage_needed);
  if (lrels == NULL) {
    perror("Failed to allocate space for relocations");
    return -1;
  }

  numrels = bfd_canonicalize_reloc(sect_src->owner, sect_src, lrels, symtab);
  if (numrels < 0) {
    free(lrels);
    bfd_perror("Failed to canonicalize relocations");
    return -1;
  }

  *rels = lrels;
  *relcount = numrels;
  return 0;
}

static void register_reloc_symusage(const size_t relcount, arelent **rels,
    const size_t symcount, asymbol **symtab, BitArray *usage) {
  size_t idx;
  arelent **lastrel = rels + relcount;
  asymbol **cursym;

  while (rels < lastrel) {
    cursym = (*rels)->sym_ptr_ptr;
    idx = cursym - symtab;
    if (idx > symcount) {
      fprintf(stderr,
          "Oops. Symbol \"%s\" not in canonical symbol table?\n",
          (*cursym)->name);
    } else {
      bitarray_set(usage, idx);
    }

    rels++;
  }
}

static size_t
get_used_undefined_symbols(const size_t symcount, asymbol **symtab,
    BitArray *usage, asymbol ***output)
{
  asection *sect;
  size_t i, retval = 0;

  for (i = 0; i < symcount; i++) {
    sect = symtab[i]->section;

    if bitarray_get(usage, i)
        printf("Symbol: %-20s, sect: %-10s:%04lx\n",
            symtab[i]->name,
            symtab[i]->section->name,
            (long unsigned int)(symtab[i]->value));
    // Is the symbol in the und or com-section _AND_ used during the relocation
    if ((bfd_is_und_section(sect) | bfd_is_com_section(sect) ) && bitarray_get(usage, i)) {
      if (output != NULL){
        output[retval] = symtab + i;
      }
      retval++;

    }
  }
  return retval;
}

#if 0
static size_t
get_longest_symname(const size_t symcount, asymbol ***symtab)
{
  size_t i, tmp, res = 0;

  for (i = 0; i < symcount; i++) {
    tmp = strlen((*(symtab[i]))->name);
    if (tmp > res) res = tmp;
  }
  return res;
}
#endif


static asymbol*
my_get_symbol_by_name(const char *name, const size_t symcount,
    asymbol **symtab)
{
  size_t i;

  for (i = 0; i < symcount; i++) {
    if (! strcmp(name, symtab[i]->name)) return symtab[i];
  }
  return NULL;
}

static int
cmp_symname(const void *a, const void *b)
{
  const asymbol *syma = **(asymbol***)a;
  const asymbol *symb = **(asymbol***)b;
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
sort_symbols_by_name(const size_t symcount, asymbol ***symtab)
{
  qsort(symtab, symcount, sizeof(*symtab), cmp_symname);
}

static int
cmp_relentoffset(const void *a, const void *b)
{
  const arelent *rela = **(arelent***)a;
  const arelent *relb = **(arelent***)b;

  if (rela->address < relb->address) return -1;
  if (rela->address == relb->address) return 0;
  return 1;
}

static void
sort_relocs_by_offset(const size_t relcount, arelent **input,
    arelent ***output)
{
  size_t i;

  //Index fields
  for (i = 0; i < relcount; i++) {
    output[i] = input + i;
  }

  //Sort them by offset
  qsort(output, relcount, sizeof(*output), cmp_relentoffset);

}

static void
inverse_map_symbols(const size_t symcount, asymbol **baseoffs,
    const size_t inpcount, asymbol ***input, size_t *output)
{
  size_t i, offs;

  for (i = 0; i < symcount; i++) {
    output[i] = symcount;
  }

  for (i = 0; i < inpcount; i++) {
    offs = input[i] - baseoffs;
    output[offs] = i;
  }
}

static int write_symbollist(const size_t symcount, asymbol ***symtab,
    FILE *stream) {
  size_t i, xlen, wres;
  uint8_t match;
  const char *curname;
  printf("Number of symbols:%i\n", symcount);

  for (i = 0; i < symcount; i++) {
    match = 0;
    if (i != 0){
      while(1){
        if((*(symtab[i -1]))->name[match] != (*(symtab[i]))->name[match]) break;
        match++;
      }

    }
    curname = (*(symtab[i]))->name;
    xlen = strlen(curname) + 1;
    xlen -= match;
    wres = fwrite(&match, 1,1,stream);
    if (wres != 1) {
      perror("Error writing match to output");
      return -1;
    }


    wres = fwrite(&(curname[match ]), 1, xlen, stream);
    printf("<%i>%s\n", match, curname);
    if (wres != xlen) {
      perror("Error writing symbol string to output");
      return -1;
    }
  }
  return 0;
}

static int write_escaped_stream(const void *ptr, size_t len, FILE* stream) {
  const unsigned char *cvals = ptr;
  size_t okdata;

  while (len) {
    for (okdata = 0; okdata < len; okdata++) {
      if (cvals[okdata] == MINILINK_RELOC_ESC)
        break;
    }

    if (okdata == len)
      break;

    okdata++;

    SFWRITE(cvals, okdata, stream);

    putc(0, stream);
    putc(0, stream);
    lstats.esc++;
    printf("Wrote escape.\n");



    cvals += okdata;
    len -= okdata;
  }

  if (len) {
    SFWRITE(cvals, len, stream);
  }
  return 0;
}


static int write_relocation(arelent *reloc, asymbol **symtab,
    const size_t symid_max, size_t *idmap, FILE *stream) {
  asymbol **symentry = reloc->sym_ptr_ptr;
  size_t outsymid;
  bfd_vma outaddr;
  unsigned char tmp;
  uint8_t ctr;





  //Check Relocation type
  if (strcmp(reloc->howto->name, RELTYPE_01_1) && strcmp(reloc->howto->name,
      RELTYPE_01_2)) {
    fprintf(stderr, "Unsupported relocation type %s.\n", reloc->howto->name);
    return -1;
  }

  if (symentry == NULL) {
    perror("ERROR: No symbol attached, assuming relocation section");
    return -1;
  }

  //If the relocation is a absolute address, we can just write it to the file
  if(bfd_is_abs_section((*symentry)->section)){
    SFWRITE(&((*symentry)->value), 2, stream);
    printf("wrote Absolute address for %s:%0lx to %0lx\n",
        (*symentry)->name,
        (long unsigned int)((*symentry)->value),
        (long unsigned int)(reloc->address));
    return 2;
  }


  //Write escape char
  tmp = MINILINK_RELOC_ESC;
  SFWRITE(&tmp, 1, stream);



  outsymid = idmap[symentry - symtab];

  printf("ADDR: %.4x ", (uint32_t)reloc->address);


  //Check whether this symbol is in the Kernel
  if(outsymid < symid_max){
    //Yes it is!
    printf("Symbol: %s, ID:%x",(*symentry)->name, outsymid);
    if( reloc->addend == 0 ){ // No offset
      outsymid += 1;
      SFWRITE(&outsymid, 2, stream);
      printf("\n");
      lstats.link_simp ++;

    } else { //We have an offset -> two words
      outsymid += 1 + symid_max;
      SFWRITE(&outsymid, 2, stream); //Write symbol
      SFWRITE(&(reloc->addend), 2 , stream); // Write offset.
      printf("--->Symid: %x Offset: %x\n", outsymid, (int)reloc->addend);
      lstats.link_comp ++;
    }
  }

  else {
    //It's in one of our sections!

    //The symbol is in text, data, bss, or mig section.
    asection *sectrel = (*symentry)->section;

    if (bfd_is_const_section(sectrel)) {
      fprintf(stderr,
          "Unexpected reference to section %s by relocation"
            " referencing symbol %s.\n", sectrel->name,
          (*symentry)->name);

      return -1;
    }

    // Lets see in which section it is

    outaddr = symid_max * 2 + 1;


    for(ctr = 0; ctr < NUMSECT; ctr++){
      if(strcmp(sectrel->name, sections[ctr].name) == 0) break;
      outaddr += sections[ctr].size;
    }
    if(ctr == NUMSECT){
      fprintf(stderr, "Referencing section %s not possible in"
            "minilink file format\n", sectrel->name);

      return -1;
    }


    printf("Sect: %5s:%04x + Symbol-offset %02x + Reloc-Offset: %02x ", sections[ctr].name, (uint16_t)outaddr, (uint16_t)(*symentry)->value, (uint16_t)reloc->addend);
    //outaddr += (unsigned char*)data;
    outaddr += (*symentry)->value + reloc->addend;
    printf("= %04x   (%s)\n", (uint16_t)outaddr, (*symentry)->name);
    lstats.reloc++;


    //Write address
    SFWRITE(&outaddr, 2, stream);
  }

  return 2;
}

static int write_reloc_stream(const size_t datalen, void *data,
    size_t reloc_count, arelent ***relocs, asymbol **symtab,
    const size_t symid_max, size_t *idmap, FILE *stream) {

  size_t i, baseoff = 0;
  int intres;
  arelent *curreloc;
  unsigned char *xdata = data;

  //Check whether the address space is big enough
  {
    uint32_t memsize;
    uint8_t ctr;
    memsize = reloc_count * 2;
    for(ctr = 0; ctr < NUMSECT; ctr++){
      memsize += sections[ctr].size;
    }
    if (memsize  > 0xFFFF) {
      perror("Address space is not big enough to save module!");
    }
  }

  memset(&lstats, 0, sizeof(lstats));



  for (i = 0; i < reloc_count; i++) {
    curreloc = *(relocs[i]);
    //Are we at the relocation?
    if (baseoff != (size_t) curreloc->address) {
      //Write data up to the relocation to the output stream
      intres = write_escaped_stream(xdata + baseoff, curreloc->address - baseoff, stream);

      if (intres < 0) return -1;
    }

    baseoff = curreloc->address;
    //Write relocation to stream
    //printf("Value: %x\n", * ((uint16_t *) (xdata + baseoff) ));
    intres = write_relocation(curreloc, symtab, symid_max, idmap, stream);

    if (intres < 0)
      return -1;

    baseoff += intres;
  }

  printf("\n\n\nNumber of relocations: %i \n", reloc_count);
  printf("Number of reloc: %i \n", lstats.reloc);
  printf("Number of link_simp: %i \n", lstats.link_simp);
  printf("Number of link_comp %i \n", lstats.link_comp);
  printf("Number of esc: %i  \n\n\n", lstats.esc);

  {
    int *ipi = &lstats.esc;
    int *ipo = &tstats.esc;
    int ctr;
    for(ctr = 0; ctr < (int)(sizeof(lstats)/sizeof(int)); ctr++){
      *ipo += *ipi;
      ipi++;
      ipo++;
    }

  }



  //Write away data behind the last relocation to the stream
  if (baseoff != datalen) {
    intres = write_escaped_stream(xdata + baseoff, datalen - baseoff,
        stream);
    if (intres < 0)
      return -1;
  }
  return 0;
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

static void
print_usage(void)
{
  fputs("mkminimod creates a loadable program for sky platform\n"
  "Usage:\n"
  "    mkminimod <input> <output>\n\n"
  "Parameters:\n"
  "    input           ELF File containing kernel\n"
  "    output          Output file to create\n\n", stderr);
}

int
main(int argc, const char *argv[])
{
  FILE *foutput = NULL;
  bfd *elfinput = NULL;
  bfd_boolean bfdres;
  int intres, retval = EXIT_FAILURE;
  size_t ffunres, symbol_count;
  size_t undefsym_count;
  asymbol *autostart_sym, **symbol_table = NULL;

  Minilink_Header headerdata;
  BitArray *symusage = NULL;
  asymbol ***undefsyms = NULL;
  size_t *symidlist = NULL;
  uint8_t ctr_sect;


  memset(&headerdata, 0, sizeof(headerdata));

  /* --- check arguments ---------------------------------- */
  if (argc != 3) {
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
    fputs("This should be an ELF file containing a compiled program\n",
      stderr);
    goto cleanup_closefiles;
  }

  foutput = fopen(argv[2], "w+b");
  if (foutput == NULL) {
    perror("Failed to open output file");
    goto cleanup_closefiles;
  }

  /* --- locate required sections ------------------------- */
  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    sections[ctr_sect].sectptr = bfd_get_section_by_name(elfinput, sections[ctr_sect].name);
    if (sections[ctr_sect].sectptr == NULL) {
      if(sections[ctr_sect].required){
        fprintf(stderr,"Unable to locate section %s. - Cancel\n",  sections[ctr_sect].name);
        goto cleanup_closefiles;
      } else {
        printf("No Section %s found.\n", sections[ctr_sect].name );
      }
    } else {
      sections[ctr_sect].available = 1;
    }
  }

   /* --- load symbol table and relocations ---------------- */

  // Load Pointers to the symbol table
  intres = load_symtab(elfinput, &symbol_count, &symbol_table);

  if (intres < 0) goto cleanup_closefiles;

  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    //Only run if section has relocations
    if(!sections[ctr_sect].has_relocations) continue;
    if(sections[ctr_sect].sectptr == NULL) continue;
    // Load relocations of the text section
    intres = load_relocations(sections[ctr_sect].sectptr, symbol_table, &(sections[ctr_sect].reloc_count), &(sections[ctr_sect].reloc));
    if (intres < 0) goto cleanup_free;

    {
      printf("\nSECTION: %i\n", ctr_sect);
      uint32_t ctr;
      for(ctr = 0; ctr < sections[ctr_sect].reloc_count; ctr++ ){
        printf("RELOC: At: %04lx  to %s \n",
            (long unsigned int)(sections[ctr_sect].reloc[ctr]->address),
            (*sections[ctr_sect].reloc[ctr]->sym_ptr_ptr)->name);
      }
    }


    // Alloc space to sort the relocations
    sections[ctr_sect].sorted_reloc = malloc(sections[ctr_sect].reloc_count * sizeof(void*));
    if (sections[ctr_sect].sorted_reloc == NULL) {
      fprintf(stderr, "Not enough memory for sorting %s relocations",  sections[ctr_sect].name);
      goto cleanup_free;
    }
    // Sort the relocations by offset, so the ones at the beginning of
    // the code come first
    sort_relocs_by_offset(sections[ctr_sect].reloc_count, sections[ctr_sect].reloc, sections[ctr_sect].sorted_reloc);
  }





  /* --- load text and data and mig section content --------------- */


  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    if(!sections[ctr_sect].has_relocations) continue;
    if(sections[ctr_sect].sectptr == NULL) continue;
    bfdres = bfd_malloc_and_get_section(elfinput, sections[ctr_sect].sectptr, &(sections[ctr_sect].content) );
      if (! bfdres) {
        bfd_perror("Failed to load section");
        goto cleanup_free;
      }
  }



  /* --- get usage statistics ----------------------------- */
  symusage = calloc((symbol_count + CHAR_BIT - 1) / CHAR_BIT, 1);
    // was: bitarray_alloc(symbol_count);

  if (symusage == NULL) {
    perror("Failed to allocate space for symbol usage stats");
    goto cleanup_free;
  }

  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
      if(!sections[ctr_sect].has_relocations) continue;
      register_reloc_symusage(sections[ctr_sect].reloc_count, sections[ctr_sect].reloc, symbol_count, symbol_table, symusage);
  }

  //Count the number of undefined symbols (output = NULL)
   undefsym_count = get_used_undefined_symbols(symbol_count, symbol_table, symusage, NULL);


  /* --- setup symbol mapping ----------------------------- */
  // Alloc memory for the undefined symbols
  undefsyms = malloc(undefsym_count * sizeof(*undefsyms));

  if (undefsyms == NULL) {
    perror("Failed to allocate space for undefined symbol list");
    goto cleanup_free;
  }

  //Allocate memory for pointers on symbols
  symidlist = malloc(symbol_count * sizeof(*symidlist));
  if (symidlist == NULL) {
    perror("Failed to allocate space for symbol id mapping");
    goto cleanup_free;
  }

  // copy the pointers of the undefined symbols into undefsyms
  get_used_undefined_symbols(symbol_count, symbol_table, symusage, undefsyms);



  // sort the undefined symbols by name so they can be found more quickly while linking
  sort_symbols_by_name(undefsym_count, undefsyms);


  inverse_map_symbols(symbol_count, symbol_table, undefsym_count, undefsyms, symidlist);

  /* --- compile and output header data ------------------- */
  //Get entry point
  autostart_sym = my_get_symbol_by_name(PROCESS_ENTRY_NAME, symbol_count, symbol_table);

  if (autostart_sym == NULL) {
    fputs("Process entry not found. Can't build module.\n", stderr);
    goto cleanup_free;
  }

  //Must be in text-section
  if (autostart_sym->section != sections[0].sectptr) {
    fputs("Process structure not within text section.\n", stderr);
    goto cleanup_free;
  }

  //assemble header
  printf("Assembling header: \n");
  headerdata.common.magic = MINILINK_PGM_MAGIC;
  printf("magic: %.4x\n", MINILINK_PGM_MAGIC);
  headerdata.common.crc = 0;

  headerdata.processoffset = autostart_sym->value;
  printf("headerdata.processoffset: %.4x\n", headerdata.processoffset);


  //Calculate sizes
  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    sections[ctr_sect].size = (sections[ctr_sect].sectptr) ? (sections[ctr_sect].sectptr->size): 0;
    printf("Section %i, Size: h%x.4\n",ctr_sect, sections[ctr_sect].size);
  }


  headerdata.textsize = sections[0].size;
  printf("headerdata.textsize: h%.4x\n", headerdata.textsize);

  headerdata.datasize = sections[1].size;
  printf("headerdata.datasize: h%.4x\n", headerdata.datasize);

  headerdata.bsssize = sections[2].size;
  printf("headerdata.bsssize: h%.4x\n", headerdata.bsssize);

  headerdata.migsize = sections[3].size;
  printf("headerdata.migsize: h%.4x\n", headerdata.migsize);

  headerdata.migptrsize = sections[4].size;
  printf("headerdata.migptrsize: h%.4x\n", headerdata.migptrsize);



  headerdata.symentries = undefsym_count;
  printf("headerdata.symentries: %.4x\n", headerdata.symentries);

  //Make sure sections are word-alligned
  if (headerdata.textsize & 1) {
    fputs("WARNING: Text section not word aligned!", stderr);
    headerdata.textsize++;
  }
  if (headerdata.datasize & 1) {
    fputs("WARNING: Data section not word aligned!", stderr);
    headerdata.datasize++;
  }

  //Convert header into little endian
  intres = convert_program_header(&headerdata, databuf, sizeof(databuf));
  if (intres < 0) {
    fputs("Internal error when serializing header data.\n", stderr);
    goto cleanup_free;
  }

  // Write header to output file
  ffunres = fwrite(databuf, 1, intres, foutput);
  if (ffunres != (size_t)intres) {
    perror("Problem writing header to output file");
    goto cleanup_free;
  }

  /* --- write symbol list -------------------------------- */
  intres = write_symbollist(undefsym_count, undefsyms, foutput);
  if (intres != 0) goto cleanup_free;

  /* --- output escaped section data ---------------------- */
  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    uint8_t lsect = (ctr_sect + 1) % NUMSECT; //Text sect last
    if(!sections[lsect].has_relocations) continue;
    if(sections[lsect].size == 0) continue;

    printf("Section %s:\n",sections[lsect].name);
    intres = write_reloc_stream(sections[lsect].sectptr->size, sections[lsect].content, sections[lsect].reloc_count,
        sections[lsect].sorted_reloc, symbol_table, undefsym_count, symidlist, foutput);

    if (intres < 0) goto cleanup_free;

    // If datasect isn't word aligned datasize has be increased by one, before.
    if (sections[lsect].sectptr->size != sections[lsect].size) {
      intres = putc(0, foutput);
      if (intres != 0) {
        perror("Failed to add padding byte");
        goto cleanup_free;
      }
    }

  }


  /* Last byte in file must not be zero, otherwise cfs-coffe won't be able
   * to determine the proper file size.
  */
  intres = putc(0xff, foutput);
  if (intres != 0xff) {
    perror("Failed to write eof marker");
    goto cleanup_free;
  }
  /* --- checksum data ------------------------------------ */
  //Rewind the output stream so we can calculate the crc32
  intres = fseek(foutput, 0, SEEK_SET);
  if (intres != 0) {
    perror("Rewinding output stream failed");
    goto cleanup_free;
  }

  //Calculate the crc over the output strem
  intres = crc32k_checksum_stream(foutput, &headerdata.common.crc);
  if (intres < 0) goto cleanup_free;

  //Go to the beginning again to write the header including the crc
  intres = fseek(foutput, 0, SEEK_SET);
  if (intres != 0) {
    perror("Rewinding output stream failed");
    goto cleanup_free;
  }

  // Convert the header to LE - this time with the right crc
  intres = convert_program_header(&headerdata, databuf, sizeof(databuf));
  if (intres < 0) {
    fputs("Internal error when serializing header data.\n", stderr);
    goto cleanup_free;
  }


  //Write the header to the file
  ffunres = fwrite(databuf, 1, intres, foutput);
  if (ffunres != (size_t)intres) {
    perror("Problem overwriting header in output file");
    goto cleanup_free;
  }

  /* --- close files -------------------------------------- */
  bfd_close(elfinput);
  elfinput = NULL;

  intres = fclose(foutput);
  if (intres != 0) {
    perror("Failed to close output file");
    foutput = NULL;
    goto cleanup_free;
  }
  foutput = NULL;
  /* --- everything went ok ------------------------------- */

  puts("\n\nTotal numbers");
  puts(    "=============");
  printf("\nNumber of relocations: %i \n", tstats.reloc + tstats.link_simp + tstats.link_comp);
  printf("Number of reloc: %i \n", tstats.reloc);
  printf("Number of link_simp: %i \n", tstats.link_simp);
  printf("Number of link_comp %i \n", tstats.link_comp);
  printf("Number of esc: %i  \n\n\n", tstats.esc);

  /* --- everything went ok ------------------------------- */
  retval = EXIT_SUCCESS;

cleanup_free:
  for(ctr_sect = 0; ctr_sect < NUMSECT; ctr_sect ++){
    free(sections[ctr_sect].sorted_reloc);
    free(sections[ctr_sect].content);
    free(sections[ctr_sect].reloc);
  }
  free(symidlist);
  free(undefsyms);
  free(symusage);
  free(symbol_table);
cleanup_closefiles:
  if (elfinput) bfd_close(elfinput);
  if (foutput)  fclose(foutput);
  return retval;
}

/* @} */
