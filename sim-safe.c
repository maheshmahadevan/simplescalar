/* sim-safe.c - sample functional simulator implementation */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"

/*
 * This file implements a functional simulator.  This functional simulator is
 * the simplest, most user-friendly simulator in the simplescalar tool set.
 * Unlike sim-fast, this functional simulator checks for all instruction
 * errors, and the implementation is crafted for clarity rather than speed.
 */

/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* track number of refs */
static counter_t sim_num_refs = 0;

/*hw2 - track number of cylces*/
static counter_t sim_num_cycle = 0;
static counter_t sim_num_cy_inst = 0;

/*hw2 - track IPC */
static counter_t sim_IPC = 0;

/* maximum number of inst's to execute */
static unsigned int max_insts;

/*hw2 8.d struct hazards*/
int struct_hazards = FALSE;
int last_inst_mem = FALSE; /*flag to check if last instruction on the cycle was mem operation*/

/*hw2 8.e strict ctrl hazards*/
int strict_ctrl_hazards = FALSE;
int last_inst_ctrl = FALSE; /*flag to check if last instruction on the cycle was ctrl operation*/

/*hw2 8.f ctrl hazards*/
int ctrl = FALSE;
int last_inst_ctrl_taken = FALSE; /*flag to check if last instruction on the cycle was ctrl inst that was taken*/

/*hw2 8.g data hazards*/
int data_hazards = FALSE;
int last_out1,last_out2; /*tmp variables to store the value of out1 and out2 register from first inst of cycle*/

/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
"sim-safe: This simulator implements a functional simulator.  This\n"
"functional simulator is the simplest, most user-friendly simulator in the\n"
"simplescalar tool set.  Unlike sim-fast, this functional simulator checks\n"
"for all instruction errors, and the implementation is crafted for clarity\n"
"rather than speed.\n"
		 );

  /* instruction limit */
  opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute",
	       &max_insts, /* default */0,
	       /* print */TRUE, /* format */NULL);

  /*hw2 - 8.d Implementing struct hazard */
  opt_reg_flag(sim_odb, "-struct_hazards", "set flag for structural hazard",
  	       &struct_hazards, /* default */FALSE, /* !print */FALSE, NULL);

/*hw2 - 8.e Implementing strict control hazard */
  opt_reg_flag(sim_odb, "-strict_ctrl_hazards", "set flag for strict control hazard",
  	       &strict_ctrl_hazards, /* default */FALSE, /* !print */FALSE, NULL);

/*hw2 - 8.f Implementing control hazard */
  opt_reg_flag(sim_odb, "-ctrl", "set flag for control hazard",
  	       &ctrl, /* default */FALSE, /* !print */FALSE, NULL);

/*hw2 - 8.g Implementing data hazard */
  opt_reg_flag(sim_odb, "-data_hazards", "set flag for data hazard",
  	       &data_hazards, /* default */FALSE, /* !print */FALSE, NULL);

}

/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  /* nada */
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, "sim_num_insn",
		   "total number of instructions executed",
		   &sim_num_insn, sim_num_insn, NULL);
  stat_reg_counter(sdb, "sim_num_refs",
		   "total number of loads and stores executed",
		   &sim_num_refs, 0, NULL);
  stat_reg_int(sdb, "sim_elapsed_time",
	       "total simulation time in seconds",
	       &sim_elapsed_time, 0, NULL);
  /*hw2 - track sim_num_cycle and sim_IPC*/
  stat_reg_counter(sdb, "sim_num_cycle",
	       "total number of cycles",
	       &sim_num_cycle, 0, NULL);
  stat_reg_formula(sdb, "sim_IPC",
	       "IPC count",
	       "sim_num_insn / sim_num_cycle", NULL);
  stat_reg_formula(sdb, "sim_inst_rate",
		   "simulation speed (in insts/sec)",
		   "sim_num_insn / sim_elapsed_time", NULL);
  ld_reg_stats(sdb);
  mem_reg_stats(mem, sdb);
}

/* initialize the simulator */
void
sim_init(void)
{
  sim_num_refs = 0;
  /*hw2 - initialize num of cycles*/	
  sim_num_cycle = 0;
  sim_IPC = 0;

  /* allocate and initialize register file */
  regs_init(&regs);

  /* allocate and initialize memory space */
  mem = mem_create("mem");
  mem_init(mem);
}

/* load program into simulated state */
void
sim_load_prog(char *fname,		/* program to load */
	      int argc, char **argv,	/* program arguments */
	      char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);

  /* initialize the DLite debugger */
  dlite_init(md_reg_obj, dlite_mem_obj, dlite_mstate_obj);
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream)		/* output stream */
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream)		/* output stream */
{
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void)
{
  /* nada */
}


/*
 * configure the instruction decode engine
 */

#define DNA			(0)

#if defined(TARGET_PISA)

/* general register dependence decoders */
#define DGPR(N)			(N)
#define DGPR_D(N)		((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)		(((N)+32)&~1)
#define DFPR_F(N)		(((N)+32)&~1)
#define DFPR_D(N)		(((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI			(0+32+32)
#define DLO			(1+32+32)
#define DFCC			(2+32+32)
#define DTMP			(3+32+32)

#elif defined(TARGET_ALPHA)

/* general register dependence decoders, $r31 maps to DNA (0) */
#define DGPR(N)			(31 - (N)) /* was: (((N) == 31) ? DNA : (N)) */

/* floating point register dependence decoders */
#define DFPR(N)			(((N) == 31) ? DNA : ((N)+32))

/* miscellaneous register dependence decoders */
#define DFPCR			(0+32+32)
#define DUNIQ			(1+32+32)
#define DTMP			(2+32+32)

#else
#error No ISA target defined...
#endif


/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(regs.regs_PC)

/* general purpose registers */
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

#if defined(TARGET_PISA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N)		(regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR)	(regs.regs_F.q[N] = (EXPR))
#define FPR(N)			(regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)		(regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR			(regs.regs_C.fpcr)
#define SET_FPCR(EXPR)		(regs.regs_C.fpcr = (EXPR))
#define UNIQ			(regs.regs_C.uniq)
#define SET_UNIQ(EXPR)		(regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void)
{
  md_inst_t inst;
  register md_addr_t addr;
  enum md_opcode op;
  register int is_write;
  enum md_fault_type fault;
  int out1, out2, in1, in2, in3;
  

  fprintf(stderr, "sim: ** starting functional simulation **\n");

  /* set up initial default next PC */
  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);

  /* check for DLite debugger entry condition */
  if (dlite_check_break(regs.regs_PC, /* !access */0, /* addr */0, 0, 0))
    dlite_main(regs.regs_PC - sizeof(md_inst_t),
	       regs.regs_PC, sim_num_insn, &regs, mem);

  while (TRUE)
    {
      /* maintain $r0 semantics */
      regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
      regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif /* TARGET_ALPHA */

      /* get the next instruction to execute */
      MD_FETCH_INST(inst, mem, regs.regs_PC);

     
      /*hw2* - keep a count on instruction for calculating cycles*/
	//sim_num_cy_inst++;	
      /* keep an instruction count */
      sim_num_insn++;
      
      /*Hw 2 - increase count of cycle*/
      if(sim_num_insn%2 == 1)
      {
	sim_num_cycle++;
	last_inst_mem = FALSE; /*hw2 8.d initialize value of flag last_inst_mem to false on every first instruction of the cycles*/
	last_inst_ctrl = FALSE; /*hw2 8.e initialize value of flag last_inst_ctrl to false on every first instruction of the cycles*/
        last_inst_ctrl_taken = FALSE; /*hw2 8.f initialize value of flag last_inst_ctrl_taken to false on every first instruction of the cycles*/
	last_out1 = 0;last_out2=0;/*hw2 8.g initialize tmp register variables on first instruction of the cycle*/				
      }

      

      /* set default reference address and access mode */
      addr = 0; is_write = FALSE;

      /* set default fault - none */
      fault = md_fault_none;

      /* decode the instruction */
      MD_SET_OPCODE(op, inst);
      /* execute the instruction */
      switch (op)
	{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:                                                        \
	  /* compute output/input dependencies to out1-2 and in1-3 */	\
	  /* out1-2/in1-3 hold register names, NA if None          */	\
	  out1 = O1; out2 = O2;						\
	  in1 = I1; in2 = I2; in3 = I3;					\
          SYMCAT(OP,_IMPL);						\
          break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
        case OP:							\
          panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)						\
	  { fault = (FAULT); break; }
#include "machine.def"
	default:
	  panic("attempted to execute a bogus opcode");
      }

      if (fault != md_fault_none)
	fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

      /*hw2 8.d if last instruction in cycle was also memory then stall this instruction*/
     if (MD_OP_FLAGS(op) & F_MEM) 
	if(last_inst_mem==TRUE && struct_hazards == TRUE)
     	{
	    //sim_num_cycle++;
	     continue;	
      	}	

      /*hw2 8.e if last instruction in cycle is ctrl inst then stall this instruction*/
    if(last_inst_ctrl==TRUE && strict_ctrl_hazards == TRUE)
     	{
	     //sim_num_cycle++;
	     continue;	
      	}

    /*hw2 8.f if last instruction in cycle is ctrl inst and taken then stall this instruction*/
    if(last_inst_ctrl_taken==TRUE && ctrl == TRUE)
     	{
	     //sim_num_insn--;	
	     continue;	
      	}
	
    /*hw2 8.g if last instruction in cycle has a output register which is used as input in current cycle then stall the instruction*/
    if(((last_out1!=0 && (last_out1 == in1 || last_out1 == in2 || last_out1 == in3)) || (last_out2 != 0 &&(last_out2 == in1 || last_out2 == in2 || last_out2 == in3))) && data_hazards == TRUE)
     	{
	    // sim_num_insn--;	
	     continue;	
      	}	

/* keep an instruction count */
     //sim_num_insn++;

      if (verbose)
	{
	  /*hw2 8.c*/
	  if(sim_num_insn%2 == 1)
	  {				
	  	fprintf(stderr,"cycle : %d \n",sim_num_cycle);
	  	
	  }
	  myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ",
		    sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
	  md_print_insn(inst, regs.regs_PC, stderr);
	  if (MD_OP_FLAGS(op) & F_MEM)
	  {
		  myfprintf(stderr, "  mem: 0x%08p", addr);
	  }
	  if (MD_OP_FLAGS(op) & F_CTRL)
	  {
		fprintf(stderr,"-->control inst");
		if(regs.regs_NPC != regs.regs_PC + 4) 
		{
			fprintf(stderr,"::taken");
		}
		else
		{
			fprintf(stderr,"::not-taken");
		}
	   }		
	  fprintf(stderr, "\n");
	  /* fflush(stderr); */
	}

      if (MD_OP_FLAGS(op) & F_MEM)
	{
	  last_inst_mem = TRUE;/*hw2 8.d set the value of flag last_inst_mem to true*/	
	  sim_num_refs++;
	  if (MD_OP_FLAGS(op) & F_STORE)
	    is_write = TRUE;
	}

      if (MD_OP_FLAGS(op) & F_CTRL)
	{
	    last_inst_ctrl = TRUE;/*hw2 8.e set the value of flag last_inst_ctrl to true*/
	    if(regs.regs_NPC != regs.regs_PC + 4) 			
	     {
		last_inst_ctrl_taken = TRUE;/*hw2 8.f set the value of flag last_inst_ctrl_taken to true*/
	      }		
	
	}

	if(data_hazards)
	{
	   last_out1 = out1;
	   last_out2 = out2;
	}	

      /* check for DLite debugger entry condition */
      if (dlite_check_break(regs.regs_NPC,
			    is_write ? ACCESS_WRITE : ACCESS_READ,
			    addr, sim_num_insn, sim_num_insn))
	dlite_main(regs.regs_PC, regs.regs_NPC, sim_num_insn, &regs, mem);

      /* go to the next instruction */
      regs.regs_PC = regs.regs_NPC;
      regs.regs_NPC += sizeof(md_inst_t);
      	

      /* finish early? */
      if (max_insts && sim_num_insn >= max_insts)
	return;
	
    }


}
        
