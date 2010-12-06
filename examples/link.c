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
 *         Adjusted c&p from a running system - not tested in this form. Please give feedback if there is trouble.
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 *         Moritz Strübe <Moritz.Struebe@informatik.uni-erlangen.de>
 */


struct process * proc_ready;

static void
proc_dequeue(struct process * proc)
{
  struct process ** lproc;
  lproc = &proc_ready;
  while (*lproc != NULL && *lproc != proc){
    lproc = &((*lproc)->next);
  }
  if(*lproc == NULL){
    return;
  }
  *lproc = (*lproc)->next;
}

static struct process *
proc_get(char * name)
{
  struct process * lproc;
  lproc = proc_ready;
  while (lproc != NULL && strcmp(lproc->name, name) != 0){
    lproc = lproc->next;
  }
  return lproc;
}

int
link(char * file)
{

  int status;
  struct process **proclist, **curproc;
  SDPUTS("link");

  status = minilink_load(file, "symbols.mls", &proclist);
  if(status == 0) {

    for (curproc = proclist; *curproc != NULL; curproc++) {
      struct process * tmp;
      //TODO this shouldn't be needed
      clock_wait(10);

      if(SIMINFO)
        printf("Attaching process %s@%x *%x\n", (*curproc)->name,
            (unsigned) (*curproc), (unsigned) curproc);

      tmp = proc_ready;
      for (tmp = proc_ready; tmp != *curproc && tmp->next != NULL; tmp
          = tmp->next);
      if(tmp->next == NULL && tmp != *curproc) {
        tmp->next = *curproc;
        (*curproc)->next = NULL;
      }
    }

  }
  return status;

}

int
start(void)
{

  struct process *proc;
  SDPUTS("Start");
  proc = proc_get(cmd.data);
  if(proc == NULL) {
    return 1;
  }
  proc_dequeue(proc);
  process_start(proc, NULL);
  return 0;

}
