/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

//#include <string.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#if IS_ENABLED(CONFIG_ARM) || IS_ENABLED(CONFIG_ARM64)
#include <asm/memory.h>
#endif
#include <asm/unistd.h>
#include "asm-generic/int-ll64.h"
#include "linux/kernel.h"
#include "linux/mm.h"
#include "linux/semaphore.h"
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>	//wake_up_process()
#include <linux/kthread.h>	//kthread_create()??ï¿½ï¿½|kthread_run()
#include <linux/err.h>		//IS_ERR()??ï¿½ï¿½|PTR_ERR()
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/init.h>
//#include <mach/sys_config.h>
//#include <mach/platform.h>

#include "EP952_If.h"
#include "DDC_If.h"
#include "EP952SettingsData.h"

#if Enable_HDCP
#include "HDCP.h"
#endif
//--------------------------------------------------------------------------------------------------

//#define Little_Endian (for linux only)

//--------------------------------------------------------------------------------------------------

// Private Data
unsigned char IIC_EP952_Addr, IIC_HDCPKey_Addr;
unsigned short TempUSHORT;
unsigned char Temp_Data[15];

// Global date for HDMI Transmiter
unsigned char is_RSEN;
unsigned char Cache_EP952_DE_Control;

// Private Functions
SMBUS_STATUS IIC_Write(unsigned char IICAddr, unsigned char ByteAddr,
		       unsigned char *Data, unsigned int Size);
SMBUS_STATUS IIC_Read(unsigned char IICAddr, unsigned char ByteAddr,
		      unsigned char *Data, unsigned int Size);

//==================================================================================================
//
// Public Function Implementation
//

//--------------------------------------------------------------------------------------------------
// Hardware Interface

void EP952_IIC_Initial(void)
{
	IIC_EP952_Addr = 0x29;	// EP952 slave address
	IIC_HDCPKey_Addr = 0xA8;	// HDCP Key (EEPROM) slave address

	// Initial Variables
	Temp_Data[0] = EP952_TX_PHY_Control_0__TERM_ON;
	EP952_Reg_Write(EP952_TX_PHY_Control_0, Temp_Data, 1);

	Temp_Data[0] = 0;
	EP952_Reg_Write(EP952_TX_PHY_Control_1, Temp_Data, 1);
}

void EP952_Info_Reset(void)
{
	int i;

	// Global date for HDMI Transmiter
	is_RSEN = 0;
	Cache_EP952_DE_Control = 0x03;

	// Initial Settings
	EP952_Reg_Set_Bit(EP952_Packet_Control, EP952_Packet_Control__VTX0);
	EP952_Reg_Set_Bit(EP952_General_Control_1,
			  EP952_General_Control_1__INT_OD);

	//
	// Set Default AVI Info Frame
	//
	memset(Temp_Data, 0x00, 14);

	// Set AVI Info Frame to RGB
	Temp_Data[1] &= 0x60;
	Temp_Data[1] |= 0x00;	// RGB

	// Set AVI Info Frame to 601
	Temp_Data[2] &= 0xC0;
	Temp_Data[2] |= 0x40;

	// Write AVI Info Frame
	Temp_Data[0] = 0;
	for (i = 1; i < 14; ++i) {
		Temp_Data[0] += Temp_Data[i];
	}
	Temp_Data[0] = ~(Temp_Data[0] - 1);
	EP952_Reg_Write(EP952_AVI_Packet, Temp_Data, 14);

	//
	// Set Default ADO Info Frame
	//
	memset(Temp_Data, 0x00, 6);

	// Write ADO Info Frame
	Temp_Data[0] = 0;
	for (i = 1; i < 6; ++i) {
		Temp_Data[0] += Temp_Data[i];
	}
	Temp_Data[0] = ~(Temp_Data[0] - 1);
	EP952_Reg_Write(EP952_ADO_Packet, Temp_Data, 6);

	//
	// Set Default CS Info Frame
	//
	memset(Temp_Data, 0x00, 5);

	EP952_Reg_Write(EP952_Channel_Status, Temp_Data, 5);
/*
	//
	// clear Packet_1 Info Frame
	//
	Temp_Data[0] = 0;
	for(i=EP952_Data_Packet_Header; i<= 0x5F; i++) {
		EP952_Reg_Write(i, Temp_Data, 1);
	}
*/
}

//--------------------------------------------------------------------------------------------------
//
// HDMI Transmiter (EP952-Tx Implementation)
//

void HDMI_Tx_Power_Down(void)
{
	// Software power down
	EP952_Reg_Clear_Bit(EP952_General_Control_1,
			    EP952_General_Control_1__PU);
	DBG_printf(("< EP952 Tx Power Down >\r\n"));
}

void HDMI_Tx_Power_Up(void)
{
	// Software power up
	EP952_Reg_Set_Bit(EP952_General_Control_1, EP952_General_Control_1__PU);
	DBG_printf(("< EP952 Tx Power Up >\r\n"));
}

unsigned char HDMI_Tx_HTPLG(void)
{
	// EP952 RSEN Enable
	EP952_Reg_Clear_Bit(EP952_TX_PHY_Control_1,
			    EP952_TX_PHY_Control_1__RESN_DIS);

	// read register
	EP952_Reg_Read(EP952_General_Control_2, Temp_Data, 1);

#if Enable_HDCP
	// check HDCP Ri Interrupt Flag
	if (Temp_Data[0] & EP952_General_Control_2__RIFE) {
		HDCP_Ext_Ri_Trigger();
	}
#endif

	// check RSEN status
	is_RSEN = (Temp_Data[0] & EP952_General_Control_2__RSEN) ? 1 : 0;

	// return HTPLG status
	if (Temp_Data[0] & EP952_General_Control_2__HTPLG) {
		return 1;
	} else {
		return 0;
	}
}

unsigned char HDMI_Tx_RSEN(void)
{
	return is_RSEN;
}

unsigned char HDMI_Tx_hpd_state(void)
{
	unsigned char temp_data[16] = { 0 };
	int result = 1;
	// EP952 RSEN Enable
	//EP952_Reg_Clear_Bit(EP952_TX_PHY_Control_1, EP952_TX_PHY_Control_1__RESN_DIS);
	result = IIC_Read(IIC_EP952_Addr, EP952_TX_PHY_Control_1, temp_data, 1);
	if (result == 0) {
		// Write back to Reg Reg_Addr
		temp_data[0] &= ~EP952_TX_PHY_Control_1__RESN_DIS;
		IIC_Write(IIC_EP952_Addr, EP952_TX_PHY_Control_1, temp_data, 1);
	}
	// read register
	EP952_Reg_Read(EP952_General_Control_2, temp_data, 1);

#if Enable_HDCP			// fixme
	// check HDCP Ri Interrupt Flag
	if (temp_data[0] & EP952_General_Control_2__RIFE) {
		HDCP_Ext_Ri_Trigger();
	}
#endif

	// return HTPLG status
	if (temp_data[0] & EP952_General_Control_2__HTPLG) {
		return 1;
	} else {
		return 0;
	}
}

void HDMI_Tx_HDMI(void)
{
	EP952_Reg_Set_Bit(EP952_General_Control_4,
			  EP952_General_Control_4__HDMI);
	DBG_printf(("EP952 Set to HDMI output mode\r\n"));
}

void HDMI_Tx_DVI(void)
{
	EP952_Reg_Clear_Bit(EP952_General_Control_4,
			    EP952_General_Control_4__HDMI);
	DBG_printf(("EP952 Set to DVI output mode\r\n"));
}

//------------------------------------
// HDCP

void HDMI_Tx_Mute_Enable(void)
{
	HDMI_Tx_AMute_Enable();
	HDMI_Tx_VMute_Enable();
}

void HDMI_Tx_Mute_Disable(void)
{
	HDMI_Tx_VMute_Disable();
	HDMI_Tx_AMute_Disable();
}

void HDMI_Tx_HDCP_Enable(void)
{
	EP952_Reg_Set_Bit(EP952_General_Control_5,
			  EP952_General_Control_5__ENC_EN);
}

void HDMI_Tx_HDCP_Disable(void)
{
	EP952_Reg_Clear_Bit(EP952_General_Control_5,
			    EP952_General_Control_5__ENC_EN);
}

void HDMI_Tx_RPTR_Set(void)
{
	EP952_Reg_Set_Bit(EP952_General_Control_5,
			  EP952_General_Control_5__RPTR);
}

void HDMI_Tx_RPTR_Clear(void)
{
	EP952_Reg_Clear_Bit(EP952_General_Control_5,
			    EP952_General_Control_5__RPTR);
}

void HDMI_Tx_write_AN(unsigned char *pAN)
{
	EP952_Reg_Write(EP952_AN, pAN, 8);
}

unsigned char HDMI_Tx_AKSV_RDY(void)
{
	status = EP952_Reg_Read(EP952_General_Control_5, Temp_Data, 1);
	if (status != SMBUS_STATUS_Success) {
		DBG_printf(("ERROR: AKSV RDY - MCU IIC %d\r\n", (int)status));
		return 0;
	}
	return (Temp_Data[0] & EP952_General_Control_5__AKSV_RDY) ? 1 : 0;
}

unsigned char HDMI_Tx_read_AKSV(unsigned char *pAKSV)
{
	int i = 0, j = 0;

	status = EP952_Reg_Read(EP952_AKSV, pAKSV, 5);
	if (status != SMBUS_STATUS_Success) {
		DBG_printf(("ERROR: AKSV read - MCU IIC %d\r\n", (int)status));
		return 0;
	}

	while (i < 5) {
		Temp_Data[0] = 1;
		while (Temp_Data[0]) {
			if (pAKSV[i] & Temp_Data[0])
				j++;
			Temp_Data[0] <<= 1;
		}
		i++;
	}

	if (j != 20) {
		DBG_printf(("ERROR: AKSV read - Key Wrong\r\n"));
		return 0;
	}

	return 1;
}

void HDMI_Tx_write_BKSV(unsigned char *pBKSV)
{
	EP952_Reg_Write(EP952_BKSV, pBKSV, 5);
}

unsigned char HDMI_Tx_RI_RDY(void)
{
	EP952_Reg_Read(EP952_General_Control_5, Temp_Data, 1);
	return (Temp_Data[0] & EP952_General_Control_5__RI_RDY) ? 1 : 0;
}

unsigned char HDMI_Tx_read_RI(unsigned char *pRI)
{
	status = EP952_Reg_Read(EP952_RI, pRI, 2);
	if (status != SMBUS_STATUS_Success) {
		DBG_printf(("ERROR: Tx Ri read - MCU IIC %d\r\n", (int)status));
		return 0;
	}
	return 1;
}

void HDMI_Tx_read_M0(unsigned char *pM0)
{
	status = EP952_Reg_Read(EP952_M0, pM0, 8);
}

SMBUS_STATUS HDMI_Tx_Get_Key(unsigned char *Key)
{
	return IIC_Read(IIC_HDCPKey_Addr, 0, Key, 512);
}

//------------------------------------
// Special for config

void HDMI_Tx_AMute_Enable(void)
{
	EP952_Reg_Set_Bit(EP952_Color_Space_Control,
			  EP952_Color_Space_Control__AMUTE);
	EP952_Reg_Set_Bit(EP952_Pixel_Repetition_Control,
			  EP952_Pixel_Repetition_Control__CTS_M);

	EP952_Reg_Clear_Bit(EP952_IIS_Control, EP952_IIS_Control__ADO_EN);
	EP952_Reg_Clear_Bit(EP952_IIS_Control, EP952_IIS_Control__AUDIO_EN);

	DBG_printf(("< EP952 Audio_Mute_enable >\r\n"));
}

void HDMI_Tx_AMute_Disable(void)
{
	EP952_Reg_Clear_Bit(EP952_Pixel_Repetition_Control,
			    EP952_Pixel_Repetition_Control__CTS_M);
	EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
			    EP952_Color_Space_Control__AVMUTE);
	EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
			    EP952_Color_Space_Control__AMUTE);

	EP952_Reg_Set_Bit(EP952_IIS_Control, EP952_IIS_Control__ADO_EN);
	EP952_Reg_Set_Bit(EP952_IIS_Control, EP952_IIS_Control__AUDIO_EN);

	DBG_printf(("< EP952 Audio_Mute_disable >\r\n"));
}

void HDMI_Tx_VMute_Enable(void)
{
	EP952_Reg_Set_Bit(EP952_Color_Space_Control,
			  EP952_Color_Space_Control__VMUTE);

	DBG_printf(("< EP952 Video_Mute_enable >\r\n"));
}

void HDMI_Tx_VMute_Disable(void)
{
	EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
			    EP952_Color_Space_Control__AVMUTE);
	EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
			    EP952_Color_Space_Control__VMUTE);

	DBG_printf(("< EP952 Video_Mute_disable >\r\n"));
}

void HDMI_Tx_Video_Config(PVDO_PARAMS Params)
{
	int i;
	DBG_printf(("\r\n ========== Update EP952 video Registers ==========\r\n"));

	// Disable auto transmission AVI packet
	EP952_Reg_Clear_Bit(EP952_IIS_Control, EP952_IIS_Control__AVI_EN);

	////////////////////////////////////////////////////////
	// Video Interface

	// De_Skew
	EP952_Reg_Read(EP952_General_Control_3, Temp_Data, 1);
	Temp_Data[0] &= ~0xF0;
	Temp_Data[0] |= Params->Interface & 0xF0;
	EP952_Reg_Write(EP952_General_Control_3, Temp_Data, 1);

	// input DSEL BSEL EDGE
	EP952_Reg_Read(EP952_General_Control_1, Temp_Data, 1);
	Temp_Data[0] &= ~0x0E;
	Temp_Data[0] |= Params->Interface & 0x0E;
	EP952_Reg_Write(EP952_General_Control_1, Temp_Data, 1);

	if (Params->Interface & 0x01) {
		EP952_Reg_Set_Bit(EP952_General_Control_4,
				  EP952_General_Control_4__FMT12);
	} else {
		EP952_Reg_Clear_Bit(EP952_General_Control_4,
				    EP952_General_Control_4__FMT12);
	}

	// update HVPol
	EP952_Reg_Read(EP952_General_Control_4, Temp_Data, 1);
	Params->HVPol =
	    Temp_Data[0] & (EP952_DE_Control__VSO_POL |
			    EP952_DE_Control__HSO_POL);

	////////////////////////////////////////////////////////
	// Sync Mode
	switch (Params->SyncMode) {
	default:
	case SYNCMODE_HVDE:
		// Disable E_SYNC
		EP952_Reg_Clear_Bit(EP952_General_Control_4,
				    EP952_General_Control_4__E_SYNC);
		// Disable DE_GEN
		Cache_EP952_DE_Control &= ~EP952_DE_Control__DE_GEN;

		// Regular VSO_POL, HSO_POL
		if ((Params->HVPol & VNegHPos) != (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VNegHPos)) {	// V
			Cache_EP952_DE_Control |= EP952_DE_Control__VSO_POL;	// Invert
		} else {
			Cache_EP952_DE_Control &= ~EP952_DE_Control__VSO_POL;
		}
		if ((Params->HVPol & VPosHNeg) != (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VPosHNeg)) {	// H
			Cache_EP952_DE_Control |= EP952_DE_Control__HSO_POL;	// Invert
		} else {
			Cache_EP952_DE_Control &= ~EP952_DE_Control__HSO_POL;
		}
		DBG_printf(("EP952 Set Sync mode to (H+V+DE)input mode\r\n"));
		break;

	case SYNCMODE_HV:
		// Disable E_SYNC
		EP952_Reg_Clear_Bit(EP952_General_Control_4,
				    EP952_General_Control_4__E_SYNC);
		// Enable DE_GEN
		Cache_EP952_DE_Control |= EP952_DE_Control__DE_GEN;

		// Regular VSO_POL, HSO_POL
		if ((Params->HVPol & VNegHPos) != (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VNegHPos)) {	// V
			Cache_EP952_DE_Control |= EP952_DE_Control__VSO_POL;	// Invert
		} else {
			Cache_EP952_DE_Control &= ~EP952_DE_Control__VSO_POL;
		}
		if ((Params->HVPol & VPosHNeg) != (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VPosHNeg)) {	// H
			Cache_EP952_DE_Control |= EP952_DE_Control__HSO_POL;	// Invert
		} else {
			Cache_EP952_DE_Control &= ~EP952_DE_Control__HSO_POL;
		}

		// Set DE generation params
		if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {
			Cache_EP952_DE_Control &= ~0x03;

#ifdef Little_Endian
			Cache_EP952_DE_Control |=
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_DLY)[1];
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_DLY)[0];
			EP952_Reg_Write(EP952_DE_DLY, Temp_Data, 1);
#else // Big Endian
			Cache_EP952_DE_Control |=
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_DLY)[0];
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_DLY)[1];
			EP952_Reg_Write(EP952_DE_DLY, Temp_Data, 1);
#endif

			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_TOP)[0];
			EP952_Reg_Write(EP952_DE_TOP, Temp_Data, 1);

#ifdef Little_Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_CNT)[0];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_CNT)[1];
			EP952_Reg_Write(EP952_DE_CNT, Temp_Data, 2);
#else // Big Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_CNT)[1];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_CNT)[0];
			EP952_Reg_Write(EP952_DE_CNT, Temp_Data, 2);
#endif

#ifdef Little_Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_LIN)[1];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_LIN)[0];
			EP952_Reg_Write(EP952_DE_LIN, Temp_Data, 2);
#else // Big Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_LIN)[1];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     DE_Gen.DE_LIN)[0];
			EP952_Reg_Write(EP952_DE_LIN, Temp_Data, 2);
#endif

			DBG_printf(("EP952 DE_GEN params (DE_DLY=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    DE_Gen.DE_DLY));
			DBG_printf((", DE_CNT=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    DE_Gen.DE_CNT));
			DBG_printf((", DE_TOP=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    DE_Gen.DE_TOP));
			DBG_printf((", DE_LIN=%u)",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    DE_Gen.DE_LIN));
			DBG_printf((")\r\n"));
		} else {
			DBG_printf(("ERROR: VideoCode overflow DE_GEN table\r\n"));
		}

		DBG_printf(("EP952 Set Sync mode to (H+V)input + DE_GEN mode\r\n"));
		break;

	case SYNCMODE_Embeded:
		// Disable DE_GEN
		Cache_EP952_DE_Control &= ~EP952_DE_Control__DE_GEN;
		// Enable E_SYNC
		EP952_Reg_Set_Bit(EP952_General_Control_4,
				  EP952_General_Control_4__E_SYNC);

		// Set E_SYNC params
		if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {

			Temp_Data[0] =
			    EP952_VDO_Settings[Params->VideoSettingIndex].
			    E_Sync.CTL;
			EP952_Reg_Write(EP952_Embedded_Sync_Control, Temp_Data,
					1);

			TempUSHORT =
			    EP952_VDO_Settings[Params->VideoSettingIndex].
			    E_Sync.H_DLY;
			if (!(Params->Interface & 0x04)) {	// Mux Mode
				TempUSHORT += 2;
			}
#ifdef Little_Endian
			Temp_Data[0] = ((unsigned char *)&TempUSHORT)[0];
			Temp_Data[1] = ((unsigned char *)&TempUSHORT)[1];
			EP952_Reg_Write(EP952_H_Delay, Temp_Data, 2);
#else // Big Endian
			Temp_Data[0] = ((unsigned char *)&TempUSHORT)[1];
			Temp_Data[1] = ((unsigned char *)&TempUSHORT)[0];
			EP952_Reg_Write(EP952_H_Delay, Temp_Data, 2);
#endif

#ifdef Little_Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.H_WIDTH)[0];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.H_WIDTH)[1];
			EP952_Reg_Write(EP952_H_Width, Temp_Data, 2);
#else // Big Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.H_WIDTH)[1];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.H_WIDTH)[0];
			EP952_Reg_Write(EP952_H_Width, Temp_Data, 2);
#endif

			Temp_Data[0] =
			    EP952_VDO_Settings[Params->VideoSettingIndex].
			    E_Sync.V_DLY;
			EP952_Reg_Write(EP952_V_Delay, Temp_Data, 1);

			Temp_Data[0] =
			    EP952_VDO_Settings[Params->VideoSettingIndex].
			    E_Sync.V_WIDTH;
			EP952_Reg_Write(EP952_V_Width, Temp_Data, 1);

#ifdef Little_Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.V_OFST)[0];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.V_OFST)[1];
			EP952_Reg_Write(EP952_V_Off_Set, Temp_Data, 2);
#else // Big Endian
			Temp_Data[0] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.V_OFST)[1];
			Temp_Data[1] =
			    ((unsigned char *)
			     &EP952_VDO_Settings[Params->VideoSettingIndex].
			     E_Sync.V_OFST)[0];
			EP952_Reg_Write(EP952_V_Off_Set, Temp_Data, 2);
#endif

			DBG_printf(("EP952 E_SYNC params (CTL=0x%02X",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.CTL));
			DBG_printf((", H_DLY=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.H_DLY));
			DBG_printf((", H_WIDTH=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.H_WIDTH));
			DBG_printf((", V_DLY=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.V_DLY));
			DBG_printf((", V_WIDTH=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.V_WIDTH));
			DBG_printf((", V_OFST=%u",
				    (unsigned short)EP952_VDO_Settings[Params->
								       VideoSettingIndex].
				    E_Sync.V_OFST));
			DBG_printf((")\r\n"));

			// Regular VSO_POL, HSO_POL
			if (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VNegHPos) {	// VNeg?
				Cache_EP952_DE_Control |=
				    EP952_DE_Control__VSO_POL;
			} else {
				Cache_EP952_DE_Control &=
				    ~EP952_DE_Control__VSO_POL;
			}
			if (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.HVPol & VPosHNeg) {	// HNeg?
				Cache_EP952_DE_Control |=
				    EP952_DE_Control__HSO_POL;
			} else {
				Cache_EP952_DE_Control &=
				    ~EP952_DE_Control__HSO_POL;
			}
		} else {
			DBG_printf(("ERROR: VideoCode overflow E_SYNC table\r\n"));
		}

		DBG_printf(("EP952 Set Sync mode to (Embeded)input mode\r\n"));
		break;
	}
	EP952_Reg_Write(EP952_DE_Control, &Cache_EP952_DE_Control, 1);

	////////////////////////////////////////////////////////
	// Pixel Repetition
	EP952_Reg_Read(EP952_Pixel_Repetition_Control, Temp_Data, 1);
	Temp_Data[0] &= ~EP952_Pixel_Repetition_Control__PR;
	if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {
		Temp_Data[0] |=
		    EP952_VDO_Settings[Params->VideoSettingIndex].AR_PR & 0x03;
	}
	EP952_Reg_Write(EP952_Pixel_Repetition_Control, Temp_Data, 1);

	////////////////////////////////////////////////////////
	// Color Space
	switch (Params->FormatIn) {
	default:
	case COLORFORMAT_RGB:
		EP952_Reg_Clear_Bit(EP952_General_Control_4,
				    EP952_General_Control_4__YCC_IN |
				    EP952_General_Control_4__422_IN);
		DBG_printf(("EP952 Set to RGB In\r\n"));
		break;
	case COLORFORMAT_YCC444:
		EP952_Reg_Set_Bit(EP952_General_Control_4,
				  EP952_General_Control_4__YCC_IN);
		EP952_Reg_Clear_Bit(EP952_General_Control_4,
				    EP952_General_Control_4__422_IN);
		DBG_printf(("EP952 Set to YCC444 In\r\n"));
		break;
	case COLORFORMAT_YCC422:
		EP952_Reg_Set_Bit(EP952_General_Control_4,
				  EP952_General_Control_4__YCC_IN |
				  EP952_General_Control_4__422_IN);
		DBG_printf(("EP952 Set to YCC422 In\r\n"));
		break;
	}
	switch (Params->FormatOut) {
	default:
	case COLORFORMAT_RGB:
		// Set to RGB
		if (Params->VideoSettingIndex < EP952_VDO_Settings_IT_Start) {	// CE Timing
			EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
					    EP952_Color_Space_Control__YCC_OUT |
					    EP952_Color_Space_Control__422_OUT);
			EP952_Reg_Set_Bit(EP952_Color_Space_Control, EP952_Color_Space_Control__YCC_Range);	// Output limit range RGB
		} else {	// IT Timing
			EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
					    EP952_Color_Space_Control__YCC_OUT |
					    EP952_Color_Space_Control__422_OUT |
					    EP952_Color_Space_Control__YCC_Range);
		}
		DBG_printf(("EP952 Set to RGB Out\r\n"));
		break;

	case COLORFORMAT_YCC444:
		// Set to YCC444
		EP952_Reg_Set_Bit(EP952_Color_Space_Control,
				  EP952_Color_Space_Control__YCC_OUT);
		EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
				    EP952_Color_Space_Control__422_OUT);
		DBG_printf(("EP952 Set to YCC444 Out\r\n"));
		break;

	case COLORFORMAT_YCC422:
		// Set to YCC422
		EP952_Reg_Set_Bit(EP952_Color_Space_Control,
				  EP952_Color_Space_Control__YCC_OUT |
				  EP952_Color_Space_Control__422_OUT);
		DBG_printf(("EP952 Set to YCC422 Out\r\n"));
		break;
	}

	// Color Space
	switch (Params->ColorSpace) {
	default:
	case COLORSPACE_601:
		// Set to 601
		EP952_Reg_Clear_Bit(EP952_Color_Space_Control,
				    EP952_Color_Space_Control__COLOR);
		DBG_printf(("EP952 Set to 601 color definition "));
		break;

	case COLORSPACE_709:
		// Set to 709
		EP952_Reg_Set_Bit(EP952_Color_Space_Control,
				  EP952_Color_Space_Control__COLOR);
		DBG_printf(("EP952 Set to 709 color definition "));
		break;
	}
	DBG_printf(("(VIC=%d) \r\n", (int)Params->VideoSettingIndex));

	///////////////////////////////////////////////////////////////////
	// AVI Info Frame
	//

	// clear AVI Info Frame
	memset(Temp_Data, 0x00, 14);

	// AVI InfoFrame Data Byte 1
	switch (Params->FormatOut) {
	default:
	case COLORFORMAT_RGB:
		Temp_Data[1] |= 0x00;	// AVI_Y1,Y0 = RGB
		break;

	case COLORFORMAT_YCC444:
		Temp_Data[1] |= 0x40;	// AVI_Y1,Y0 = YCC 444
		break;

	case COLORFORMAT_YCC422:
		Temp_Data[1] |= 0x20;	// AVI_Y1,Y0 = YCC 422
		break;
	}
	Temp_Data[1] |= 0x10;	// AVI_A0 = Active Format Information valid

	//SCAN
	switch (Params->SCAN) {
	default:
	case 0:
		Temp_Data[1] &= ~0x03;	// AVI_S1,S0 = No Data
		break;
	case 1:		// AVI_S1,S0 = overscan
		Temp_Data[1] |= 0x01;
		break;
	case 2:		// AVI_S1,S0 = underscan
		Temp_Data[1] |= 0x02;
		break;
	}

	// AVI InfoFrame Data Byte 2
	switch (Params->ColorSpace) {
	default:
	case COLORSPACE_601:
		Temp_Data[2] |= 0x40;	// AVI_C1,C0 = 601
		break;

	case COLORSPACE_709:
		Temp_Data[2] |= 0x80;	// AVI_C1,C0 = 709
		break;
	}

	if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {
		Temp_Data[2] |= EP952_VDO_Settings[Params->VideoSettingIndex].AR_PR & 0x30;	// AVI_M1,M0 : Picture Aspect Ratio
	}
	Temp_Data[2] |= Params->AFARate & 0x0F;	// AVI_R3~0: Active format Aspect Ratio

	// AVI InfoFrame Data Byte 3 is 0

	// AVI InfoFrame Data Byte 4 is VIC
	if (Params->VideoSettingIndex < EP952_VDO_Settings_IT_Start) {
		Temp_Data[4] |= EP952_VDO_Settings[Params->VideoSettingIndex].VideoCode;	// AVI_VIC6~0 : Vedio Identification code
	}
	// AVI InfoFrame Data Byte 5
	if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {
		Temp_Data[5] |= (EP952_VDO_Settings[Params->VideoSettingIndex].AR_PR & 0x0C) >> 2;	// AVI_PR3~0 : Pixel Repetition
	}
	// AVI InfoFrame Data Byte 0 is checksum
	Temp_Data[0] = 0x91;
	for (i = 1; i < 6; ++i) {
		Temp_Data[0] += Temp_Data[i];
	}
	Temp_Data[0] = ~(Temp_Data[0] - 1);	// checksum

	// Write AVI InfoFrame Data Byte
	EP952_Reg_Write(EP952_AVI_Packet, Temp_Data, 14);

	// print for debug
	DBG_printf(("EP952 set AVI Info: "));
	for (i = 0; i < 6; ++i) {
		DBG_printf(("[%d]0x%0.2X, ", (int)i, (int)Temp_Data[i]));
	}
	DBG_printf(("\r\n"));

	// Enable auto transmission AVI packet
	EP952_Reg_Set_Bit(EP952_IIS_Control,
			  EP952_IIS_Control__AVI_EN | EP952_IIS_Control__GC_EN);

}

void HDMI_Tx_Audio_Config(PADO_PARAMS Params)
{
	int i;
	unsigned char N_CTS_Index;
	unsigned long N_Value, CTS_Value;
	ADSFREQ FinalFrequency;
	unsigned char FinalADSRate;

	DBG_printf(("\r\n ========== Update EP952 Audio Registers ==========\r\n"));

	////////////////////////////////////////////////////////
	// Audio Settings

	// Update WS_M, WS_POL, SCK_POL
	EP952_Reg_Read(EP952_IIS_Control, Temp_Data, 1);
	Temp_Data[0] &= ~0x07;	//clear WS_M, WS_POL, SCK_POL
	Temp_Data[0] |= Params->Interface & 0x07;	//set WS_M, WS_POL, SCK_POL
	EP952_Reg_Write(EP952_IIS_Control, Temp_Data, 1);

	////////////////////////////////////////////////////////
	// IIS or SPDIF
	if (Params->Interface & 0x08) {	// IIS
		DBG_printf(("EP952 set to IIS IN \r\n"));
		Temp_Data[0] = 0;

		// Update Flat = 0
		EP952_Reg_Clear_Bit(EP952_Packet_Control,
				    EP952_Packet_Control__FLAT);

		// Power down OSC
		EP952_Reg_Set_Bit(EP952_General_Control_1,
				  EP952_General_Control_1__OSC_PD);

		// Set to IIS Input
		Temp_Data[0] =
		    EP952_General_Control_8__ADO_IIS_IN |
		    EP952_General_Control_8__COMR_DIS;
		EP952_Reg_Write(EP952_General_Control_8, Temp_Data, 1);

		// Downsample Convert
		FinalADSRate = Params->ADSRate;
		switch (Params->ADSRate) {
		default:
		case 0:	// Bypass
			//DBG_printf(("Audio ADS = 0\r\n"));
			FinalADSRate = 0;
			FinalFrequency = Params->InputFrequency;
			break;
		case 1:	// 1/2
			//DBG_printf(("Audio ADS = 1_2\r\n"));
			switch (Params->InputFrequency) {
			default:	// Bypass
				//DBG_printf(("Audio ADS = 0\r\n"));
				FinalADSRate = 0;
				FinalFrequency = Params->InputFrequency;
				break;
			case ADSFREQ_88200Hz:
				FinalFrequency = ADSFREQ_44100Hz;
				break;
			case ADSFREQ_96000Hz:
				FinalFrequency = ADSFREQ_48000Hz;
				break;
			case ADSFREQ_176400Hz:
				FinalFrequency = ADSFREQ_88200Hz;
				break;
			case ADSFREQ_192000Hz:
				FinalFrequency = ADSFREQ_96000Hz;
				break;
			}
			break;
		case 2:	// 1/3
			//DBG_printf(("Audio ADS = 1_3\r\n"));
			switch (Params->InputFrequency) {
			default:	// Bypass
				//DBG_printf(("Audio ADS = 0\r\n"));
				FinalADSRate = 0;
				FinalFrequency = Params->InputFrequency;
				break;
			case ADSFREQ_96000Hz:
				FinalFrequency = ADSFREQ_32000Hz;
				break;
			}
			break;
		case 3:	// 1/4
			//DBG_printf(("Audio ADS = 1_4\r\n"));
			switch (Params->InputFrequency) {
			default:	// Bypass
				//DBG_printf(("Audio ADS = 0\r\n"));
				FinalADSRate = 0;
				FinalFrequency = Params->InputFrequency;
				break;
			case ADSFREQ_176400Hz:
				FinalFrequency = ADSFREQ_44100Hz;
				break;
			case ADSFREQ_192000Hz:
				FinalFrequency = ADSFREQ_48000Hz;
				break;
			}
			break;
		}

		// Update Audio Down Sample (ADSR)
		EP952_Reg_Read(EP952_Pixel_Repetition_Control, Temp_Data, 1);
		Temp_Data[0] &= ~0x30;
		Temp_Data[0] |= (FinalADSRate << 4) & 0x30;
		EP952_Reg_Write(EP952_Pixel_Repetition_Control, Temp_Data, 1);

		///////////////////////////////////////////////////////////////
		// Channel Status

		memset(Temp_Data, 0x00, 5);

		Temp_Data[0] = (Params->NoCopyRight) ? 0x04 : 0x00;
		Temp_Data[1] = 0x00;	// Category code ??
		Temp_Data[2] = 0x00;	// Channel number ?? | Source number ??
		Temp_Data[3] = FinalFrequency;	// Clock accuracy ?? | Sampling frequency
		Temp_Data[4] = 0x01;	// Original sampling frequency ?? | Word length ??
		EP952_Reg_Write(EP952_Channel_Status, Temp_Data, 5);

		// print for debug
		DBG_printf(("EP952 set CS Info: "));
		for (i = 0; i < 5; ++i) {
			DBG_printf(("0x%02X, ", (int)Temp_Data[i]));
		}
		DBG_printf(("\r\n"));

		// set CS_M = 1 (use channel status regiater)
		EP952_Reg_Set_Bit(EP952_Pixel_Repetition_Control,
				  EP952_Pixel_Repetition_Control__CS_M);
	} else {		// SPIDIF
		DBG_printf(("EP952 set to SPDIF IN \r\n"));

		// power up OSC
		EP952_Reg_Clear_Bit(EP952_General_Control_1,
				    EP952_General_Control_1__OSC_PD);

		// Set SPDIF in
		Temp_Data[0] =
		    EP952_General_Control_8__ADO_SPDIF_IN |
		    EP952_General_Control_8__COMR_DIS;
		EP952_Reg_Write(EP952_General_Control_8, Temp_Data, 1);

		// Update Flat = 0
		EP952_Reg_Clear_Bit(EP952_Packet_Control,
				    EP952_Packet_Control__FLAT);

		// No Downsample
		FinalADSRate = 0;
		FinalFrequency = Params->InputFrequency;

		// Disable Down Sample and Bypass Channel Status
		EP952_Reg_Clear_Bit(EP952_Pixel_Repetition_Control,
				    EP952_Pixel_Repetition_Control__ADSR |
				    EP952_Pixel_Repetition_Control__CS_M);

		Params->ChannelNumber = 0;
	}

	////////////////////////////////////////////////////////
	// Set CTS/N
	if (Params->VideoSettingIndex < EP952_VDO_Settings_Max) {
		N_CTS_Index =
		    EP952_VDO_Settings[Params->VideoSettingIndex].Pix_Freq_Type;
		if (EP952_VDO_Settings[Params->VideoSettingIndex].HVRes_Type.Vprd % 500) {	// 59.94/60 Hz
			N_CTS_Index += Params->VFS;
			//DBG_printf(("EP952 Use N_CTS_Index shift(VFS) = %d\r\n", (int)Params->VFS));
		}
	} else {
		DBG_printf(("EP952 Use default N_CTS_Index\r\n"));
		N_CTS_Index = PIX_FREQ_25200KHz;
	}

	switch (FinalFrequency) {

	default:
	case ADSFREQ_32000Hz:
		DBG_printf(("EP952 Set to 32KHz"));
		N_Value = N_CTS_32K[N_CTS_Index].N;
		CTS_Value = N_CTS_32K[N_CTS_Index].CTS;
		break;
	case ADSFREQ_44100Hz:
		DBG_printf(("EP952 Set to 44.1KHz"));
		N_Value = N_CTS_44K1[N_CTS_Index].N;
		CTS_Value = N_CTS_44K1[N_CTS_Index].CTS;
		break;
	case ADSFREQ_48000Hz:
		DBG_printf(("EP952 Set to 48KHz"));
		N_Value = N_CTS_48K[N_CTS_Index].N;
		CTS_Value = N_CTS_48K[N_CTS_Index].CTS;
		break;
	case ADSFREQ_88200Hz:
		DBG_printf(("EP952 Set to 88.2KHz"));
		N_Value = N_CTS_44K1[N_CTS_Index].N * 2;
		CTS_Value = N_CTS_44K1[N_CTS_Index].CTS * 2;
		break;
	case ADSFREQ_96000Hz:
		DBG_printf(("EP952 Set to 96KHz"));
		N_Value = N_CTS_48K[N_CTS_Index].N * 2;
		CTS_Value = N_CTS_48K[N_CTS_Index].CTS * 2;
		break;
	case ADSFREQ_176400Hz:
		DBG_printf(("EP952 Set to 176.4KHz"));
		N_Value = N_CTS_44K1[N_CTS_Index].N * 4;
		CTS_Value = N_CTS_44K1[N_CTS_Index].CTS * 4;
		break;
	case ADSFREQ_192000Hz:
		DBG_printf(("EP952 Set to 192KHz"));
		N_Value = N_CTS_48K[N_CTS_Index].N * 4;
		CTS_Value = N_CTS_48K[N_CTS_Index].CTS * 4;
		break;
	}

	// write to EP952 - CTS.N value
	Temp_Data[0] = CTS_Value >> 16;
	EP952_Reg_Write(EP952_CTS_H, Temp_Data, 1);
	Temp_Data[0] = CTS_Value >> 8;
	EP952_Reg_Write(EP952_CTS_M, Temp_Data, 1);
	Temp_Data[0] = CTS_Value;
	EP952_Reg_Write(EP952_CTS_L, Temp_Data, 1);

	Temp_Data[0] = N_Value >> 16;
	EP952_Reg_Write(EP952_N_H, Temp_Data, 1);
	Temp_Data[0] = N_Value >> 8;
	EP952_Reg_Write(EP952_N_M, Temp_Data, 1);
	Temp_Data[0] = N_Value;
	EP952_Reg_Write(EP952_N_L, Temp_Data, 1);

	DBG_printf((" table[%d]: N=%ld, CTS=%ld (VIC=%d)\r\n", (int)N_CTS_Index,
		    N_Value, CTS_Value, (int)Params->VideoSettingIndex));

	/*
	   // for debug
	   EP952_Reg_Read(EP952_CTS_H, Temp_Data, 1);
	   DBG_printf(("EP952_CTS_0(Reg addr 0x60) = 0x%02X\r\n",(int)Temp_Data[0]));
	   EP952_Reg_Read(EP952_CTS_M, Temp_Data, 1);
	   DBG_printf(("EP952_CTS_1(Reg addr 0x61) = 0x%02X\r\n",(int)Temp_Data[0]));
	   EP952_Reg_Read(EP952_CTS_L, Temp_Data, 1);
	   DBG_printf(("EP952_CTS_2(Reg addr 0x62) = 0x%02X\r\n",(int)Temp_Data[0]));

	   EP952_Reg_Read(EP952_N_H, Temp_Data, 1);
	   DBG_printf(("EP952_N_0(Reg addr 0x63) = 0x%02X\r\n",(int)Temp_Data[0]));
	   EP952_Reg_Read(EP952_N_M, Temp_Data, 1);
	   DBG_printf(("EP952_N_1(Reg addr 0x64) = 0x%02X\r\n",(int)Temp_Data[0]));
	   EP952_Reg_Read(EP952_N_L, Temp_Data, 1);
	   DBG_printf(("EP952_N_2(Reg addr 0x65) = 0x%02X\r\n",(int)Temp_Data[0]));
	 */

	//////////////////////////////////////////////////////
	// ADO InfoFrame
	//

	// clear Default ADO InfoFrame
	memset(Temp_Data, 0x00, 6);

	// Overwrite ADO InfoFrame
	Temp_Data[1] = Params->ChannelNumber;
	Temp_Data[4] = EP952_ADO_Settings[Params->ChannelNumber].SpeakerMapping;

	// ADO InfoFrame data byte 0 is checksum
	Temp_Data[0] = 0x8F;
	for (i = 1; i < 6; ++i) {
		Temp_Data[0] += Temp_Data[i];
	}
	Temp_Data[0] = ~(Temp_Data[0] - 1);

	// Write ADO Info Frame back
	EP952_Reg_Write(EP952_ADO_Packet, Temp_Data, 6);

	// print for Debug
	DBG_printf(("EP952 set ADO Info: "));
	for (i = 0; i < 6; ++i) {
		DBG_printf(("[%d]0x%0.2X, ", (int)i, (int)Temp_Data[i]));
	}
	DBG_printf(("\r\n"));

	// enable ADO packet
	EP952_Reg_Set_Bit(EP952_IIS_Control,
			  EP952_IIS_Control__ACR_EN | EP952_IIS_Control__ADO_EN
			  | EP952_IIS_Control__GC_EN |
			  EP952_IIS_Control__AUDIO_EN);
}

//--------------------------------------------------------------------------------------------------
//
// Hardware Interface
//

SMBUS_STATUS EP952_Reg_Read(unsigned char ByteAddr, unsigned char *Data,
			    unsigned int Size)
{
	return IIC_Read(IIC_EP952_Addr, ByteAddr, Data, Size);
}

SMBUS_STATUS EP952_Reg_Write(unsigned char ByteAddr, unsigned char *Data,
			     unsigned int Size)
{
	//DBG_printf(("EP952_Reg_Write 0x%02X, 0x%02X\r\n",(int)ByteAddr,(int)Data[0]));
	return IIC_Write(IIC_EP952_Addr, ByteAddr, Data, Size);
}

SMBUS_STATUS EP952_Reg_Set_Bit(unsigned char ByteAddr, unsigned char BitMask)
{
	int result = 1;
	result = IIC_Read(IIC_EP952_Addr, ByteAddr, Temp_Data, 1);
	if (result == 0) {
		// Write back to Reg Reg_Addr
		Temp_Data[0] |= BitMask;

		return IIC_Write(IIC_EP952_Addr, ByteAddr, Temp_Data, 1);
	} else {
		return result;
	}
}

SMBUS_STATUS EP952_Reg_Clear_Bit(unsigned char ByteAddr, unsigned char BitMask)
{
	int result = 1;
	result = IIC_Read(IIC_EP952_Addr, ByteAddr, Temp_Data, 1);
	if (result == 0) {
		// Write back to Reg Reg_Addr
		Temp_Data[0] &= ~BitMask;

		return IIC_Write(IIC_EP952_Addr, ByteAddr, Temp_Data, 1);
	} else {
		return result;
	}
}

//==================================================================================================
//
// Private Functions
//
extern s32 ep952_i2c_write(u32 client_addr, u8 *data, int size);
extern s32 ep952_i2c_read(u32 client_addr, u8 sub_addr, u8 *data, int size);

SMBUS_STATUS IIC_Write(unsigned char IICAddr, unsigned char ByteAddr,
		       unsigned char *Data, unsigned int Size)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// return 0; for success
	// return 2; for No_ACK
	// return 4; for Arbitration
	/////////////////////////////////////////////////////////////////////////////////////////////////
	unsigned char datas[32] = { 0 };

	if (Size > 31) {
		printk("iic-wirte size(%d) > 31\n", Size);
		return 4;
	}
	datas[0] = ByteAddr;
	memcpy((void *)(datas + 1), (void *)Data, Size);
	return ep952_i2c_write(IICAddr, datas, Size + 1);

}

SMBUS_STATUS IIC_Read(unsigned char IICAddr, unsigned char ByteAddr,
		      unsigned char *Data, unsigned int Size)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// return 0; for success
	// return 2; for No_ACK
	// return 4; for Arbitration
	/////////////////////////////////////////////////////////////////////////////////////////////////

	return ep952_i2c_read(IICAddr, ByteAddr, Data, Size);

}
