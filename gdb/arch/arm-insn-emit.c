/* Copyright (C) 2015-2016 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "common-defs.h"
#include "arm.h"
#include "arm-insn-utils.h"
#include "arm-insn-emit.h"

#define ABS(x) ((x) < 0 ? - (x) : (x))

/* See arm-insn-emit.h. */

uint16_t
repeat_bit(uint8_t bit, uint8_t from, uint8_t length)
{
  uint16_t value = 0;
  int i;

  for (i = from; i - from < length; i++)
    {
      value |= ENCODE (bit & 1, 1, i);
    }

  return value;
}

/* See arm-insn-emit.h.  */

uint16_t
encode_register_list (uint8_t from, uint8_t length, uint16_t initial)
{
  return repeat_bit (1, from, length) | initial;
}

/* See arm-insn-emit.h.  */

struct arm_operand
immediate_operand (uint32_t imm)
{
  struct arm_operand operand;

  operand.type = OPERAND_IMMEDIATE;
  operand.imm = imm;

  return operand;
}

/* Helper function to create a register operand, for instructions with
   different types of operands.

   For example:
   p += emit_mov (p, x0, register_operand (x1));  */

/* See arm-insn-emit.h.  */

struct arm_operand
register_operand (uint8_t reg)
{
  struct arm_operand operand;

  operand.type = OPERAND_REGISTER;
  operand.reg = reg;

  return operand;
}

/* See arm-insn-emit.h.  */

struct arm_operand
memory_operand (struct arm_memory_operand mem)
{
  struct arm_operand operand;

  operand.type = OPERAND_MEMORY;
  operand.mem = mem;

  return operand;
}

/* Write a 32-bit unsigned integer INSN info *BUF.  Return the number of
   instructions written (aka. 1).  */

static int
arm_emit_arm_insn (uint32_t *buf, uint32_t insn)
{
  *buf = insn;
  return 1;
}

/* Write a 16-bit unsigned integer INSN info *BUF.  Return the number of
   instructions written (aka. 1).  */

static int
arm_emit_thumb_insn (uint16_t *buf, uint16_t insn)
{
  *buf = insn;
  return 1;
}

/* Write a 32-bit unsigned integer INSN representing a Thumb-2 wide
   instruction info *BUF.  Return the number of instructions written
   (aka. 2).  */

static int
arm_emit_thumb_w_insn (uint16_t *buf, uint32_t insn)
{
  int res = arm_emit_thumb_insn (buf, bits (insn, 16, 31));
  res += arm_emit_thumb_insn (buf+res, bits (insn, 0, 15));
  return res;
}

/* See arm-insn-emit.h.  */

uint32_t
arm_arm_branch_relative_distance (CORE_ADDR from, CORE_ADDR to)
{
  return arm_arm_branch_adjusted_offset ((uint32_t) to - (uint32_t) from);
}

/* See arm-insn-emit.h.  */

uint32_t
arm_thumb_branch_relative_distance (CORE_ADDR from, CORE_ADDR to)
{
  uint32_t from_ = ((uint32_t) from) & ~1;
  uint32_t to_   = ((uint32_t) to) & ~1;
  return arm_thumb_branch_adjusted_offset (to_ - from_);
}

/* See arm-insn-emit.h.  */

uint32_t
arm_thumb_to_arm_branch_relative_distance (CORE_ADDR from, CORE_ADDR to)
{
  uint32_t from_ = ((uint32_t) from) & ~3;
  uint32_t to_   = ((uint32_t) to) & ~3;
  return arm_thumb_branch_adjusted_offset (to_ - from_);
}

/* See arm-insn-emit.h.  */

int
arm_arm_is_reachable (CORE_ADDR from, CORE_ADDR to)
{
  int32_t rel = arm_arm_branch_relative_distance (from, to);
  rel >>= 25;
  return !rel || !(rel + 1);
}

/* See arm-insn-emit.h.  */

int
arm_thumb_is_reachable (CORE_ADDR from, CORE_ADDR to)
{
  int32_t rel = arm_thumb_branch_relative_distance (from, to);
  rel >>= 24;
  return !rel || !(rel + 1);
}

/* See arm-insn-emit.h.  */

uint32_t
arm_arm_branch_adjusted_offset (uint32_t offset)
{
  return offset - 8;
}

/* See arm-insn-emit.h.  */

uint32_t
arm_thumb_branch_adjusted_offset (uint32_t offset)
{
  return offset - 4;
}

/* See arm-insn-emit.h.  */

uint16_t *
arm_emit_thumb_mov_32 (uint16_t *mem, int reg, uint32_t val)
{
  uint16_t val_low = bits (val, 0, 15);
  uint16_t val_high = bits (val, 16, 31);

  mem += arm_emit_thumb_movw (mem, reg, immediate_operand (val_low));
  mem += arm_emit_thumb_movt (mem, reg, immediate_operand (val_high));

  return mem;
}

/* See arm-insn-emit.h.  */

uint32_t *
arm_emit_arm_mov_32 (uint32_t *mem, int reg, uint32_t val)
{
  uint16_t val_low = bits (val, 0, 15);
  uint16_t val_high = bits (val, 16, 31);

  mem += arm_emit_arm_movw (mem, AL, reg, immediate_operand (val_low));
  mem += arm_emit_arm_movt (mem, AL, reg, immediate_operand (val_high));

  return mem;
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_branch (uint32_t *buf, enum arm_condition_codes cond,
		     struct arm_operand operand,
		     uint8_t l, uint8_t x)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      /* BLX  */
      if (l == 1 && x == 1)
	{
	  cond = 0xF;
	  l = bit (operand.imm, 1);
	}
      /* Encode B, BL or BLX.  */
      return arm_emit_arm_insn (buf, ARM_B
				| ENCODE (l, 1, 24)
				| ENCODE (cond, 4, 28)
				| ENCODE (operand.imm >> 2, 24, 0));
    }
  else if (operand.type == OPERAND_REGISTER)
    {
      if (l == 1 && x == 1)
	{
	  return arm_emit_arm_insn (buf, ARM_BLX
				    | ENCODE (cond, 4, 28)
				    | ENCODE (operand.reg, 4, 0));
	}
      else
	{
	  /* error. Only BLX has a register operand.  */
	  return 0;
	}
    }
  else
    {
      /* error.  */
      return 0;
    }
}

int
arm_emit_thumb_branch (uint16_t *buf, struct arm_operand operand,
		       uint8_t l, uint8_t x)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      uint32_t imm10, imm11;
      uint32_t s, j1, j2;

      if (x == 0)
	imm11 = bits (operand.imm, 1, 11);
      else
	/* IMM10L:H.  */
	imm11 = bits (operand.imm, 2, 11) << 1;

      imm10 = bits (operand.imm, 12, 21);
      s = bit (operand.imm, 24);
      j1 = s ^ !(bit (operand.imm, 23));
      j2 = s ^ !(bit (operand.imm, 22));

      return arm_emit_thumb_w_insn (buf, THUMB_BW
				    | ENCODE (s, 1, 26)
				    | ENCODE (imm10, 10, 16)
				    | ENCODE (l, 1, 14)
				    | ENCODE (j1, 1, 13)
				    | ENCODE (x == 0 ? 1 : 0, 1, 12)
				    | ENCODE (j2, 1, 11)
				    | ENCODE (imm11, 11, 0));
    }
  else if (operand.type == OPERAND_REGISTER)
    {
      if (l == 1 && x == 1)
        return arm_emit_thumb_insn (buf, THUMB_BLX
				    | ENCODE (operand.reg, 4, 3));
      else
	{
	  /* error.  */
	  return 0;
	}
    }
  else
    {
      /* error.  */
      return 0;
    }
}


/* See arm-insn-emit.h.  */

int
arm_emit_arm_b (uint32_t *buf, enum arm_condition_codes cond,
		uint32_t rel)
{
  return arm_emit_arm_branch (buf, cond, immediate_operand (rel), 0, 0);
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_bl (uint32_t *buf, enum arm_condition_codes cond,
		 uint32_t rel)
{
  return arm_emit_arm_branch (buf, cond, immediate_operand (rel), 1, 0);
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_bl (uint16_t *buf, uint32_t rel)
{
  return arm_emit_thumb_branch (buf, immediate_operand (rel), 1, 0);
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_b (uint16_t *buf, enum arm_condition_codes cond,
		  uint32_t rel)
{
  return arm_emit_thumb_insn (buf, THUMB_B
			      | ENCODE (cond, 4, 8)
			      | ENCODE (rel >> 1, 8, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_bw (uint16_t *buf, uint32_t rel)
{
  return arm_emit_thumb_branch (buf, immediate_operand (rel), 0, 0);
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_bw_cond (uint16_t *buf, enum arm_condition_codes cond,
			uint32_t rel)
{
  uint32_t imm6, imm11;
  uint32_t s, j1, j2;

  imm11 = bits (rel, 1, 11);
  imm6 = bits (rel, 12, 17);
  s = bit (rel, 24);
  j1 = s ^ !(bit (rel, 23));
  j2 = s ^ !(bit (rel, 22));

  return arm_emit_thumb_w_insn (buf, THUMB_BW
				| ENCODE (s, 1, 26)
				| ENCODE (imm6, 6, 16)
				| ENCODE (cond, 4, 22)
				| ENCODE (j1, 1, 13)
				| ENCODE (j2, 1, 11)
				| ENCODE (imm11, 11, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_blx (uint16_t *buf, struct arm_operand operand)
{
  return arm_emit_thumb_branch (buf, operand, 1, 1);
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_blx (uint32_t *buf, enum arm_condition_codes cond,
		  struct arm_operand operand)
{
  return arm_emit_arm_branch (buf, cond, operand, 1, 1);
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_movw (uint32_t *buf, enum arm_condition_codes cond,
		   uint8_t rd,
		   struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_MOVW
				| ENCODE (cond, 4, 28)
				| ENCODE (bits (operand.imm, 12, 15), 4, 16)
				| ENCODE (rd, 4, 12)
				| ENCODE (bits (operand.imm, 0, 11), 12, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_mov (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rd,
		  struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_MOV
				| ENCODE (cond, 4, 28)
				/* Immediate value opcode.  */
				| ENCODE (1, 1, 25)
				| ENCODE (rd, 4, 12)
				| ENCODE (bits (operand.imm, 0, 11), 12, 0));
    }
  else if (operand.type == OPERAND_REGISTER)
    {
      return arm_emit_arm_insn (buf, ARM_MOV
				| ENCODE (cond, 4, 28)
				| ENCODE (rd, 4, 12)
				| ENCODE (operand.reg, 4, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_movw (uint16_t *buf,
		     uint8_t rd,
		     struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_MOVW
				    | ENCODE (bit (operand.imm, 11), 1, 26)
				    | ENCODE (bits (operand.imm, 12, 15), 4, 16)
				    | ENCODE (bits (operand.imm, 8, 10), 3, 12)
				    | ENCODE (rd, 4, 8)
				    | ENCODE (bits (operand.imm, 0, 7), 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_movt (uint32_t *buf, enum arm_condition_codes cond,
		   uint8_t rd,
		   struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_MOVT
				| ENCODE (cond, 4, 28)
				| ENCODE (bits (operand.imm, 12, 15), 4, 16)
				| ENCODE (rd, 4, 12)
				| ENCODE (bits (operand.imm, 0, 11), 12, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_movt (uint16_t *buf,
		     uint8_t rd,
		     struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_MOVT
				    | ENCODE (bit (operand.imm, 11), 1, 26)
				    | ENCODE (bits (operand.imm, 12, 15), 4, 16)
				    | ENCODE (bits (operand.imm, 8, 10), 3, 12)
				    | ENCODE (rd, 4, 8)
				    | ENCODE (bits (operand.imm, 0, 7), 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_vpush (uint32_t *buf, enum arm_condition_codes cond,
		    uint8_t rs,
		    uint8_t len)
{
  return arm_emit_arm_insn (buf, ARM_VPUSH
			    | ENCODE (cond, 4, 28)
			    | ENCODE (bit (rs, 4), 1, 22)
			    | ENCODE (bits (rs, 0, 3), 4, 12)
			    | ENCODE (2 * len, 8, 0));

}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_vpush (uint16_t *buf,
		      uint8_t rs,
		      uint8_t len)
{
  return arm_emit_thumb_w_insn (buf, THUMB_VPUSH
				| ENCODE (bit (rs, 4), 1, 22)
				| ENCODE (bits (rs, 0, 3), 4, 12)
				| ENCODE (2 * len, 8, 0));

}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_push_list (uint32_t *buf,
			enum arm_condition_codes cond,
			uint16_t register_list)
{
    return arm_emit_arm_insn (buf, ARM_PUSH_A1
			    | ENCODE (cond, 4, 28)
			    | ENCODE (register_list, 16, 0));

}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_push_one (uint32_t *buf,
		       enum arm_condition_codes cond,
		       uint8_t rt)
{
    return arm_emit_arm_insn (buf, ARM_PUSH_A2
			    | ENCODE (cond, 4, 28)
			    | ENCODE (rt, 4, 12));

}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_push_one (uint16_t *buf, uint8_t register_list, uint8_t lr)
{
  return arm_emit_thumb_insn (buf, THUMB_PUSH_T1
			      | ENCODE (register_list, 8, 0)
			      | ENCODE (lr, 1, 8));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_push_list (uint16_t *buf,
			  uint16_t register_list,
			  uint8_t lr)
{
  return arm_emit_thumb_w_insn (buf, THUMB_PUSH_T2
				| ENCODE (bit (lr, 0), 1, 14)
				| ENCODE (register_list, 13, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_mrs (uint32_t *buf,
		  enum arm_condition_codes cond,
		  uint8_t rd)
{
  return arm_emit_arm_insn (buf, ARM_MRS
			    | ENCODE (cond, 4, 28)
			    | ENCODE (rd, 4, 12));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_mrs (uint16_t *buf, uint8_t rd)
{
  return arm_emit_thumb_w_insn (buf, THUMB_MRS | ENCODE (rd, 4, 12));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_mov (uint16_t *buf, uint8_t rd, struct arm_operand operand)
{
   if (operand.type == OPERAND_REGISTER)
    {
      return arm_emit_thumb_insn (buf, THUMB_MOV
				  | ENCODE (bit (rd, 3), 1, 7)
				  | ENCODE (bits (rd, 0, 2), 3, 0)
				  | ENCODE (operand.reg, 4, 3));
    }
   else
     {
       /* error.  */
       return 0;
     }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_dmb (uint32_t *buf)
{
  return arm_emit_arm_insn (buf, ARM_DMB | ENCODE (0xF, 4, 0));

}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_dmb (uint16_t *buf)
{
  return arm_emit_thumb_w_insn (buf, THUMB_DMB | ENCODE (0xF, 4, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_ldrex (uint32_t *buf, enum arm_condition_codes cond,
		    uint8_t rt, uint8_t rn)
{
  return arm_emit_arm_insn (buf, ARM_LDREX
			    | ENCODE (cond, 4, 28)
			    | ENCODE (rn, 4, 16)
			    | ENCODE (rt, 4, 12));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_ldrex (uint16_t *buf, int8_t rt, uint8_t rn,
			  struct arm_operand operand)
{

  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_LDREX
				    | ENCODE (rn, 4, 16)
				    | ENCODE (rt, 4, 12)
				    | ENCODE (bits (operand.imm, 0, 7), 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_cmp (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rn, struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_CMP
				| ENCODE (cond, 4, 28)
				| ENCODE (rn, 4, 16)
				| ENCODE (bits (operand.imm, 0, 11), 12, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_cmp (uint16_t *buf, uint8_t rn, struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_insn (buf, THUMB_CMP
				  | ENCODE (rn, 3, 8)
				  | ENCODE (bits (operand.imm, 0, 7), 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_cmpw (uint16_t *buf, uint8_t rn, struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_CMPW
				    | ENCODE (rn, 4, 16)
				    | ENCODE (bit (operand.imm, 11), 1, 26)
				    | ENCODE (bits (operand.imm, 8, 10),
					      3, 12)
				    | ENCODE (bits (operand.imm, 0, 7), 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_bic (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rd, uint8_t rn, struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_BIC
				| ENCODE (cond, 4, 28)
				| ENCODE (rn, 4, 16)
				| ENCODE (rd, 4, 12)
				| ENCODE (bits (operand.imm, 0, 11), 12, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_bic (uint16_t *buf, uint8_t rd, uint8_t rn,
		    struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_BIC
				    | ENCODE (bit (operand.imm, 11), 1, 26)
				    | ENCODE (rn, 4, 16)
				    | ENCODE (rd, 4, 8)
				    | ENCODE (bits (operand.imm, 8, 10),
					      3, 12)
				    | ENCODE (bits (operand.imm, 0, 7),
					      8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_strex (uint32_t *buf, enum arm_condition_codes cond,
		    uint8_t rd, uint8_t rt, uint8_t rn)
{
  return arm_emit_arm_insn (buf, ARM_STREX
			    | ENCODE (cond, 4, 28)
			    | ENCODE (rn, 4, 16)
			    | ENCODE (rd, 4, 12)
			    | ENCODE (rt, 4, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_strex (uint16_t *buf, uint8_t rd, uint8_t rt, uint8_t rn,
		      struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_w_insn (buf, THUMB_STREX
				    | ENCODE (rn, 4, 16)
				    | ENCODE (rt, 4, 12)
				    | ENCODE (rd, 4, 8)
				    | ENCODE (operand.imm, 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_str (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rt, uint8_t rn, struct arm_operand operand)
{
  if (operand.type == OPERAND_MEMORY)
    {
      switch (operand.mem.type)
	{
	case MEMORY_OPERAND_OFFSET:
	  {
	    return arm_emit_arm_insn (buf, ARM_STR
				      | ENCODE (cond, 4, 28)
				      /* P.  */
				      | ENCODE (1, 1, 24)
				      /* U.  */
				      | ENCODE (operand.mem.index < 0 ? 0 : 1,
						1, 23)
				      | ENCODE (rn, 4, 16)
				      | ENCODE (rt, 4, 12)
				      | ENCODE (bits (ABS (operand.mem.index),
						      0, 11), 12, 0));

	  }
	default:
	  {
	    /* error.  */
	    return 0;
	  }
	}
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_str (uint16_t *buf, uint8_t rt, uint8_t rn,
		    struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_insn (buf, THUMB_STR
				  | ENCODE (operand.imm, 5, 6)
				  | ENCODE (rn, 3, 3)
				  | ENCODE (rt, 3, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }

}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_add (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rd, uint8_t rn,
		  struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_arm_insn (buf, ARM_ADD
				/* Immediate operand */
				| ENCODE (1, 1, 25)
				| ENCODE (cond, 4, 28)
				/* Don't update the conditional flags.  */
				| ENCODE (0, 1, 20)
				| ENCODE (rn, 4, 16)
				| ENCODE (rd, 4, 12)
				| ENCODE (operand.imm, 8, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_add_sp (uint16_t *buf, struct arm_operand operand)
{
  if (operand.type == OPERAND_IMMEDIATE)
    {
      return arm_emit_thumb_insn (buf, THUMB_ADD_SP
				  | ENCODE (operand.imm >> 2, 7, 0));
    }
  else
    {
      /* error.  */
      return 0;
    }
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_pop_one (uint32_t *buf, enum arm_condition_codes cond,
		      uint8_t rt)
{
  return arm_emit_arm_insn (buf, ARM_POP_A2
			    | ENCODE (cond, 4, 28)
			    | ENCODE (rt, 4, 12));
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_pop_list (uint32_t *buf, enum arm_condition_codes cond,
		       uint16_t register_list)
{
  return arm_emit_arm_insn (buf, ARM_POP_A1
			    | ENCODE (cond, 4, 28)
			    | ENCODE (register_list, 16, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_pop (uint16_t *buf, uint8_t register_list, uint8_t pc)
{
  return arm_emit_thumb_insn (buf, THUMB_POP
			      | ENCODE (pc, 1, 8)
			      | ENCODE (register_list, 8, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_popw_list (uint16_t *buf, uint16_t register_list, uint8_t pc,
			  uint8_t lr)
{
  return arm_emit_thumb_w_insn (buf, THUMB_POPW
				| ENCODE (pc, 1, 15)
				| ENCODE (lr, 1, 14)
				| ENCODE (register_list, 13, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_msr (uint32_t *buf, enum arm_condition_codes cond,
		  uint8_t rn)
{
  return arm_emit_arm_insn (buf, ARM_MSR
			    | ENCODE (cond, 4, 28)
			    /* Mask 0b11  */
			    | ENCODE (3, 2, 18)
			    | ENCODE (rn, 4, 0));
}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_msr (uint16_t *buf, uint8_t rn)
{
  return arm_emit_thumb_w_insn (buf, THUMB_MSR
				| ENCODE (rn, 4, 16)
				/* Mask 0b11  */
				| ENCODE (3, 2, 10));
}

/* See arm-insn-emit.h.  */

int
arm_emit_arm_vpop (uint32_t *buf, enum arm_condition_codes cond,
		    uint8_t rs,
		    uint8_t len)
{
  return arm_emit_arm_insn (buf, ARM_VPOP
			    | ENCODE (cond, 4, 28)
			    | ENCODE (bit (rs, 4), 1, 22)
			    | ENCODE (bits (rs, 0, 3), 4, 12)
			    | ENCODE (2 * len, 8, 0));

}

/* See arm-insn-emit.h.  */

int
arm_emit_thumb_vpop (uint16_t *buf,
		      uint8_t rs,
		      uint8_t len)
{
  return arm_emit_thumb_w_insn (buf, THUMB_VPOP
				| ENCODE (bit (rs, 4), 1, 22)
				| ENCODE (bits (rs, 0, 3), 4, 12)
				| ENCODE (2 * len, 8, 0));
}
