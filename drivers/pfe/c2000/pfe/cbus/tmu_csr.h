/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/
#ifndef _TMU_CSR_H_
#define _TMU_CSR_H_

#define TMU_VERSION			(TMU_CSR_BASE_ADDR + 0x000)
#define TMU_INQ_WATERMARK		(TMU_CSR_BASE_ADDR + 0x004)
#define TMU_PHY_INQ_PKTPTR		(TMU_CSR_BASE_ADDR + 0x008)
#define TMU_PHY_INQ_PKTINFO		(TMU_CSR_BASE_ADDR + 0x00c)
#define TMU_PHY_INQ_FIFO_CNT		(TMU_CSR_BASE_ADDR + 0x010)
#define TMU_SYS_GENERIC_CONTROL		(TMU_CSR_BASE_ADDR + 0x014)
#define TMU_SYS_GENERIC_STATUS		(TMU_CSR_BASE_ADDR + 0x018)
#define TMU_SYS_GEN_CON0		(TMU_CSR_BASE_ADDR + 0x01c)
#define TMU_SYS_GEN_CON1		(TMU_CSR_BASE_ADDR + 0x020)
#define TMU_SYS_GEN_CON2		(TMU_CSR_BASE_ADDR + 0x024)
#define TMU_SYS_GEN_CON3		(TMU_CSR_BASE_ADDR + 0x028)
#define TMU_SYS_GEN_CON4		(TMU_CSR_BASE_ADDR + 0x02c)
#define TMU_TEQ_DISABLE_DROPCHK		(TMU_CSR_BASE_ADDR + 0x030)
#define TMU_TEQ_CTRL			(TMU_CSR_BASE_ADDR + 0x034)
#define TMU_TEQ_QCFG			(TMU_CSR_BASE_ADDR + 0x038)
#define TMU_TEQ_DROP_STAT		(TMU_CSR_BASE_ADDR + 0x03c)
#define TMU_TEQ_QAVG			(TMU_CSR_BASE_ADDR + 0x040)
#define TMU_TEQ_WREG_PROB		(TMU_CSR_BASE_ADDR + 0x044)
#define TMU_TEQ_TRANS_STAT		(TMU_CSR_BASE_ADDR + 0x048)
#define TMU_TEQ_HW_PROB_CFG0		(TMU_CSR_BASE_ADDR + 0x04c)
#define TMU_TEQ_HW_PROB_CFG1		(TMU_CSR_BASE_ADDR + 0x050)
#define TMU_TEQ_HW_PROB_CFG2		(TMU_CSR_BASE_ADDR + 0x054)
#define TMU_TEQ_HW_PROB_CFG3		(TMU_CSR_BASE_ADDR + 0x058)
#define TMU_TEQ_HW_PROB_CFG4		(TMU_CSR_BASE_ADDR + 0x05c)
#define TMU_TEQ_HW_PROB_CFG5		(TMU_CSR_BASE_ADDR + 0x060)
#define TMU_TEQ_HW_PROB_CFG6		(TMU_CSR_BASE_ADDR + 0x064)
#define TMU_TEQ_HW_PROB_CFG7		(TMU_CSR_BASE_ADDR + 0x068)
#define TMU_TEQ_HW_PROB_CFG8		(TMU_CSR_BASE_ADDR + 0x06c)
#define TMU_TEQ_HW_PROB_CFG9		(TMU_CSR_BASE_ADDR + 0x070)
#define TMU_TEQ_HW_PROB_CFG10		(TMU_CSR_BASE_ADDR + 0x074)
#define TMU_TEQ_HW_PROB_CFG11		(TMU_CSR_BASE_ADDR + 0x078)
#define TMU_TEQ_HW_PROB_CFG12		(TMU_CSR_BASE_ADDR + 0x07c)
#define TMU_TEQ_HW_PROB_CFG13		(TMU_CSR_BASE_ADDR + 0x080)
#define TMU_TEQ_HW_PROB_CFG14		(TMU_CSR_BASE_ADDR + 0x084)
#define TMU_TEQ_HW_PROB_CFG15		(TMU_CSR_BASE_ADDR + 0x088)
#define TMU_TEQ_HW_PROB_CFG16		(TMU_CSR_BASE_ADDR + 0x08c)
#define TMU_TEQ_HW_PROB_CFG17		(TMU_CSR_BASE_ADDR + 0x090)
#define TMU_TEQ_HW_PROB_CFG18		(TMU_CSR_BASE_ADDR + 0x094)
#define TMU_TEQ_HW_PROB_CFG19		(TMU_CSR_BASE_ADDR + 0x098)
#define TMU_TEQ_HW_PROB_CFG20		(TMU_CSR_BASE_ADDR + 0x09c)
#define TMU_TEQ_HW_PROB_CFG21		(TMU_CSR_BASE_ADDR + 0x0a0)
#define TMU_TEQ_HW_PROB_CFG22		(TMU_CSR_BASE_ADDR + 0x0a4)
#define TMU_TEQ_HW_PROB_CFG23		(TMU_CSR_BASE_ADDR + 0x0a8)
#define TMU_TEQ_HW_PROB_CFG24		(TMU_CSR_BASE_ADDR + 0x0ac)
#define TMU_TEQ_HW_PROB_CFG25		(TMU_CSR_BASE_ADDR + 0x0b0)
#define TMU_TDQ_IIFG_CFG		(TMU_CSR_BASE_ADDR + 0x0b4)
#define TMU_TDQ0_SCH_CTRL		(TMU_CSR_BASE_ADDR + 0x0b8)	/**< [9:0] Scheduler Enable for each of the scheduler in the TDQ. This is a global Enable for all schedulers in PHY0 */
#define TMU_LLM_CTRL			(TMU_CSR_BASE_ADDR + 0x0bc)
#define TMU_LLM_BASE_ADDR		(TMU_CSR_BASE_ADDR + 0x0c0)
#define TMU_LLM_QUE_LEN			(TMU_CSR_BASE_ADDR + 0x0c4)
#define TMU_LLM_QUE_HEADPTR		(TMU_CSR_BASE_ADDR + 0x0c8)
#define TMU_LLM_QUE_TAILPTR		(TMU_CSR_BASE_ADDR + 0x0cc)
#define TMU_LLM_QUE_DROPCNT		(TMU_CSR_BASE_ADDR + 0x0d0)
#define TMU_INT_EN			(TMU_CSR_BASE_ADDR + 0x0d4)
#define TMU_INT_SRC			(TMU_CSR_BASE_ADDR + 0x0d8)
#define TMU_INQ_STAT			(TMU_CSR_BASE_ADDR + 0x0dc)
#define TMU_CTRL			(TMU_CSR_BASE_ADDR + 0x0e0)

#define TMU_MEM_ACCESS_ADDR		(TMU_CSR_BASE_ADDR + 0x0e4)	/**< [31] Mem Access Command. 0 = Internal Memory Read, 1 = Internal memory Write [27:24] Byte Enables of the Internal memory access [23:0] Address of the internal memory. This address is used to access both the PM and DM of all the PE's */
#define TMU_MEM_ACCESS_WDATA		(TMU_CSR_BASE_ADDR + 0x0e8)	/**< Internal Memory Access Write Data */
#define TMU_MEM_ACCESS_RDATA		(TMU_CSR_BASE_ADDR + 0x0ec)	/**< Internal Memory Access Read Data. The commands are blocked at the mem_access only */

#define TMU_PHY0_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x0f0)	/**< [31:0] PHY0 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */
#define TMU_PHY1_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x0f4)	/**< [31:0] PHY1 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */
#define TMU_PHY2_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x0f8)	/**< [31:0] PHY2 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */
#define TMU_PHY3_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x0fc)	/**< [31:0] PHY3 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */
#define TMU_BMU_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x100)
#define TMU_TX_CTRL			(TMU_CSR_BASE_ADDR + 0x104)

#define TMU_BUS_ACCESS_WDATA		(TMU_CSR_BASE_ADDR + 0x108)
#define TMU_BUS_ACCESS			(TMU_CSR_BASE_ADDR + 0x10c)
#define TMU_BUS_ACCESS_RDATA		(TMU_CSR_BASE_ADDR + 0x110)

#define TMU_PE_SYS_CLK_RATIO		(TMU_CSR_BASE_ADDR + 0x114)
#define TMU_PE_STATUS			(TMU_CSR_BASE_ADDR + 0x118)
#define TMU_TEQ_MAX_THRESHOLD		(TMU_CSR_BASE_ADDR + 0x11c)
#define TMU_PHY4_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x134)	/**< [31:0] PHY4 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */
#define TMU_TDQ1_SCH_CTRL		(TMU_CSR_BASE_ADDR + 0x138)	/**< [9:0] Scheduler Enable for each of the scheduler in the TDQ. This is a global Enable for all schedulers in PHY1 */
#define TMU_TDQ2_SCH_CTRL		(TMU_CSR_BASE_ADDR + 0x13c)	/**< [9:0] Scheduler Enable for each of the scheduler in the TDQ. This is a global Enable for all schedulers in PHY2 */
#define TMU_TDQ3_SCH_CTRL		(TMU_CSR_BASE_ADDR + 0x140)	/**< [9:0] Scheduler Enable for each of the scheduler in the TDQ. This is a global Enable for all schedulers in PHY3 */
#define TMU_BMU_BUF_SIZE		(TMU_CSR_BASE_ADDR + 0x144)
#define TMU_PHY5_INQ_ADDR		(TMU_CSR_BASE_ADDR + 0x148)	/**< [31:0] PHY5 in queue address (must be initialized with one of the xxx_INQ_PKTPTR cbus addresses) */

#define SW_RESET	(1 << 0)	/**< Global software reset */
#define INQ_RESET	(1 << 2)
#define TEQ_RESET	(1 << 3)
#define TDQ_RESET	(1 << 4)
#define PE_RESET	(1 << 5)
#define MEM_INIT	(1 << 6)
#define MEM_INIT_DONE	(1 << 7)
#define LLM_INIT	(1 << 8)
#define LLM_INIT_DONE	(1 << 9)

typedef struct {
	u32 pe_sys_clk_ratio;
	u32 llm_base_addr;
	u32 llm_queue_len;
} TMU_CFG;

/* Not HW related for pfe_ctrl / pfe common defines */
#define DEFAULT_MAX_QDEPTH	80
#define DEFAULT_Q0_QDEPTH	511 //We keep one large queue for host tx qos
#define DEFAULT_TMU3_QDEPTH	127


#endif /* _TMU_CSR_H_ */
