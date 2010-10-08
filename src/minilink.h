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



#ifndef INCLUDED_MINILINK__H__
#define INCLUDED_MINILINK__H__

#include <stdint.h>
#include <stdlib.h>



#define MINILINK_PGM_MAGIC  0x4d4c
#define MINILINK_SYM_MAGIC  0x5359
#define MINILINK_INST_MAGIC 0x7887
#define MINILINK_RELOC_ESC  0xf5
#define MINILINK_MAX_FILENAME 16
#define MINILINK_MAX_SYMLEN 32

/** File signature for kernel image */
#define MINILINK_KERNEL_SIGNATURE       0x2A6BU
/** File signature for loaded application image */
#define MINILINK_APPLICATION_SIGNATURE  0x2A6CU

#define SYMTAB_ENTRIES_RESERVED 2

#define MINILINK_SEC 5

#define MINILINK_TEXT 0
#define MINILINK_DATA 1
#define MINILINK_BSS 2
#define MINILINK_MIG 3
#define MINILINK_MIGPTR 4


#ifndef COMPILE_HOSTED_TOOLS
#define PACK __attribute__((__packed__))
#else
#define PACK
#endif

typedef struct{
  uint16_t magic PACK; /**< Magic to identify as symbol file */
  uint32_t crc PACK;   /**< CRC32K value of complete file, with this field assumed zero */

} Minilink_CommonHeader;

typedef struct{
  Minilink_CommonHeader common PACK;
  uint32_t kernelchksum PACK;
} Minilink_SymbolHeader;

typedef struct{
  Minilink_CommonHeader common PACK;  /**< Common header information */
  uint16_t processoffset PACK; /**< Offset in ROM where process structure can be found */
  uint16_t textsize PACK;      /**< Size of text segment */
  uint16_t datasize PACK;      /**< Size of data segment to load into RAM */
  uint16_t bsssize PACK;       /**< Size of MINILINK_BSS area in RAM */
  uint16_t migsize PACK;       /**< Size of migratable area in RAM */
  uint16_t migptrsize PACK;    /**< Size of migratable pointee area in RAM */
  uint16_t symentries PACK;    /**< Number of symbols in file */
} Minilink_Header;


#undef PACK


typedef struct{

  uint16_t magic;   /**< Magic to identify as program header */
  uint32_t crc;     /**< CRC32K of original source module */
  struct {
      void * ptr;
      uint16_t size;
  } mem[5];

  void *process;   /**< Pointer to process list */
  char sourcefile[MINILINK_MAX_FILENAME]; /**< Name of file the program was loaded from */
} Minilink_ProgramInfoHeader;


#ifndef COMPILE_HOSTED_TOOLS
#include <sys/process.h>
const char * minilink_get_filename(struct process *process);
uint_fast8_t minilink_load(const char *programfile, const char *symtabfile,
    struct process ***process);
struct process *clean_minilink_space(void);
int minilink_is_process(struct process *process);
void minilink_init(void);
Minilink_ProgramInfoHeader * minilink_programm_ih(struct process *proc);
#endif

#endif /* INCLUDED_MINILINK__H__ */

/** @} */
