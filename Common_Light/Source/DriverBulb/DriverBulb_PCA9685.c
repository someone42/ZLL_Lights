/*
 * DriverBulb_PCA9685.c
 *
 *  Created on: 21/12/2017
 *      Author: Chris
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <jendefs.h>
#include <AppHardwareApi.h>
#include <stdio.h>
#include <string.h>
#include "App_MultiLight.h"
#include "DriverBulb.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define REG_MODE1			(0x00)
#define REG_MODE2			(0x01)
#define REG_SUBADR1			(0x02)
#define REG_SUBADR2			(0x03)
#define REG_SUBADR3			(0x04)
#define REG_ALLCALLADR		(0x05)
#define REG_LEDx_ON_L		(0x06)
#define REG_LEDx_ON_H		(0x07)
#define REG_LEDx_OFF_L		(0x08)
#define REG_LEDx_OFF_H		(0x09)
#define REG_LEDx_STRIDE		(4)
#define REG_ALL_LED_ON_L	(0xfa)
#define REG_ALL_LED_ON_H	(0xfb)
#define REG_ALL_LED_OFF_L	(0xfc)
#define REG_ALL_LED_OFF_H	(0xfd)
#define REG_PRE_SCALE		(0xfe)
#define REG_TESTMODE		(0xff)

#define PCA9685_ADDRESS		(0x40)		/* 7 bit default I2C address of PCA9685 */

#define FAST_DIV_BY_255(x)	((((x) << 8) + (x) + 255) >> 16)

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE void DriverBulb_vOutput(uint8 u8Bulb);
PRIVATE void PCA9685_vWriteRegister(uint8 u8Reg, uint8 u8Data);
PRIVATE void PCA9685_vWriteRegisterMulti(uint8 u8Reg, uint8 *pu8Data, uint8 u8Len);

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
PRIVATE bool_t  bIsOn[NUM_BULBS];
PRIVATE uint8   u8CurrLevel[NUM_BULBS];
PRIVATE uint8   u8CurrRed[NUM_BULBS];
PRIVATE uint8   u8CurrGreen[NUM_BULBS];
PRIVATE uint8   u8CurrBlue[NUM_BULBS];

/* Map of bulbs to PCA9685 channels. */
PRIVATE const uint8 u8ChannelMap[NUM_BULBS * 3] = {
		7,   255, 255,  /* W1 */
		6,   255, 255,  /* W2 */
		5,   255, 255,  /* W3 */
		4,   3,   2,    /* R1, G1, B1 */
		1,   0,   11,   /* R2, G2, B2 */
		10,  9,   8     /* R3, G3, B3 */
};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME:       		DriverBulb_vInit
 *
 * DESCRIPTION:		Initializes the JN516X's I2C system and initializes the
 *                  PCA9685
 *
 * PARAMETERS:      Name     RW  Usage
 *
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vInit(void)
{
	static bool_t bInit = FALSE;
	uint8 i;

	/* Not already initialized ? */
	if (bInit == FALSE)
	{
		/* Set all DIOs to be inputs, except for DIO11, which is connected to
		 * a green indicator LED */
		vAHI_DioSetDirection(0xfffff7ff, 0x00000800);

		/* Set DIO11 low i.e. switch LED on */
		vAHI_DioSetOutput(0x00000000, 0xffffffff);

		/* Initialize Serial Interface (a.k.a. I2C):
		 * enable pulse suppression filter, set prescaler to 7, so that I2C bus
		 * operates at 400 kHz */
		vAHI_SiMasterConfigure(TRUE, FALSE, 7);

		/* Put PCA9685 into SLEEP mode, before setting pre-scale register. */
		PCA9685_vWriteRegister(REG_MODE1, 0x10);

		/* Set pre-scale value to the minimum value of 3. With an internal
		 * oscillator of 25 MHz, this should result in a PWM frequency of
		 * 1526 Hz. */
		PCA9685_vWriteRegister(REG_PRE_SCALE, 0x03);

		/* Initialize PCA9685 by taking it out of SLEEP mode - other options are:
		 * restart disabled, use internal clock, register auto-increment enabled,
		 * I2C subaddresses/all call disabled */
		PCA9685_vWriteRegister(REG_MODE1, 0x20);

		/* Ensure that PCA9685 outputs are configured to be push-pull */
		PCA9685_vWriteRegister(REG_MODE2, 0x04);

		/* Note light is on */
		for (i = 0; i < NUM_BULBS; i++)
		{
			bIsOn[i] = TRUE;
			u8CurrLevel[i] = CLD_LEVELCONTROL_MAX_LEVEL;
			u8CurrRed[i] = 255;
			u8CurrGreen[i] = 255;
			u8CurrBlue[i] = 255;
			/* Set outputs */
			DriverBulb_vOutput(i);
		}

		/* Now initialized */
		bInit = TRUE;
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vOn
 *
 * DESCRIPTION:     Turns a bulb on
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn on
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vOn(uint8 u8Bulb)
{
	/* Lamp is not on ? */
	if (bIsOn[u8Bulb] != TRUE)
	{
		/* Note light is on */
		bIsOn[u8Bulb] = TRUE;
		/* Set outputs */
		DriverBulb_vOutput(u8Bulb);
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vOff
 *
 * DESCRIPTION:     Turns a bulb off
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn off
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vOff(uint8 u8Bulb)
{
	/* Lamp is on ? */
	if (bIsOn[u8Bulb] == TRUE)
	{
		/* Note light is off */
		bIsOn[u8Bulb] = FALSE;
		/* Set outputs */
		DriverBulb_vOutput(u8Bulb);
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vSetOnOff
 *
 * DESCRIPTION:     Turns a bulb off
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn on or off
 *                  bOn      R   Whether to turn bulb on (TRUE) or off (FALSE)
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vSetOnOff(uint8 u8Bulb, bool_t bOn)
{
	(bOn) ? DriverBulb_vOn(u8Bulb) : DriverBulb_vOff(u8Bulb);
}

/****************************************************************************
 *
 * NAMES:           DriverBulb_bOn
 *
 * DESCRIPTION:		Access functions for Monitored Lamp Parameters
 *
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to query
 *
 * RETURNS:
 * Lamp state
 *
 ****************************************************************************/
PUBLIC bool_t DriverBulb_bOn(uint8 u8Bulb)
{
	return (bIsOn[u8Bulb]);
}

/****************************************************************************
 *
 * NAME:       		DriverBulb_vSetLevel
 *
 * DESCRIPTION:		Sets brightness level of a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number to set level of
 *         	        u32Level R   Light level 0-LAMP_LEVEL_MAX
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vSetLevel(uint8 u8Bulb, uint32 u32Level)
{
	/* Different value ? */
	if (u8CurrLevel[u8Bulb] != (uint8) u32Level)
	{
		/* Note the new level */
		if (u32Level > CLD_LEVELCONTROL_MAX_LEVEL)
		{
			u8CurrLevel[u8Bulb] = CLD_LEVELCONTROL_MAX_LEVEL;
		}
		else
		{
			u8CurrLevel[u8Bulb] = (uint8) MAX(1, u32Level);
		}
		/* Is the lamp on ? */
		if (bIsOn[u8Bulb])
		{
			/* Set outputs */
			DriverBulb_vOutput(u8Bulb);
		}
	}
}

/****************************************************************************
 *
 * NAME:       		DriverBulb_vSetColour
 *
 * DESCRIPTION:		Updates colour of a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number to set colour of
 *         	        u32Red   R   Relative red amount
 *         	        u32Green R   Relative green amount
 *         	        u32Blue  R   Relative blue amount
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/

PUBLIC void DriverBulb_vSetColour(uint8 u8Bulb, uint32 u32Red, uint32 u32Green, uint32 u32Blue)
{
	/* Different value ? */
	if ((u8CurrRed[u8Bulb] != (uint8) u32Red)
	 || (u8CurrGreen[u8Bulb] != (uint8) u32Green)
	 || (u8CurrBlue[u8Bulb] != (uint8) u32Blue))
	{
		/* Note the new values */
		u8CurrRed[u8Bulb]   = (uint8) MIN(u32Red, 255);
		u8CurrGreen[u8Bulb] = (uint8) MIN(u32Green, 255);
		u8CurrBlue[u8Bulb]  = (uint8) MIN(u32Blue, 255);
		/* Is the lamp on ? */
		if (bIsOn[u8Bulb])
		{
			/* Set outputs */
			DriverBulb_vOutput(u8Bulb);
		}
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME:			DriverBulb_vOutput
 *
 * DESCRIPTION:     Tell PCA9685 to update PWM channels for a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb to update
 *
 *
 * RETURNS:         void
 *
 ****************************************************************************/
PRIVATE void DriverBulb_vOutput(uint8 u8Bulb)
{
	uint32  v;
	uint16  u16PWM;
	uint8   u8Brightness[3];
	int8    i;
	uint8   u8Channel;
	uint8   u8NumChannels;
	bool_t  bIsRGB;
	uint8   u8Data[4];

	bIsRGB = u8Bulb >= NUM_MONO_LIGHTS;
	u8NumChannels = bIsRGB ? 3 : 1;

	/* Is bulb on ? */
	if (bIsOn[u8Bulb])
	{
		if (bIsRGB)
		{
			/* Scale colour for brightness level */
			v = (uint32)u8CurrRed[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[0] = (uint8)FAST_DIV_BY_255(v);
			v = (uint32)u8CurrGreen[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[1] = (uint8)FAST_DIV_BY_255(v);
			v = (uint32)u8CurrBlue[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[2] = (uint8)FAST_DIV_BY_255(v);
		}
		else
		{
			u8Brightness[0] = u8CurrLevel[u8Bulb];
		}

		for (i = 0; i < u8NumChannels; i++)
		{
			/* Don't allow fully off, as PCA9685 doesn't like it when the
			 * ON and OFF count registers are the same */
			if (u8Brightness[i] == 0) u8Brightness[i] = 1;
			/* Determine which channel of PCA9685 to adjust */
			u8Channel = u8ChannelMap[u8Bulb * 3 + i];
			if (u8Brightness[i] >= CLD_LEVELCONTROL_MAX_LEVEL)
			{
				PCA9685_vWriteRegister(REG_LEDx_ON_H + u8Channel * REG_LEDx_STRIDE, 0x10); // full ON
				PCA9685_vWriteRegister(REG_LEDx_OFF_H + u8Channel * REG_LEDx_STRIDE, 0x00); // disable full OFF
			}
			else
			{
				uint16 u16On, u16Off;

				/* Set PWM duty cycle */
				u16PWM = (uint16)u8Brightness[i] << 4;
				/* Add a channel-dependent offset to ON/OFF times so that the
				 * power supply isn't hammered at count = 0 */
				u16On = (uint16)u8Channel * 256;
				u16Off = (u16On + u16PWM) & 0xfff;
				u8Data[0] = (uint8_t)u16On;                   /* REG_LEDx_ON_L */
				u8Data[1] = (uint8_t)((u16On >> 8) & 0x0f);   /* REG_LEDx_ON_H */
				u8Data[2] = (uint8_t)u16Off;                  /* REG_LEDx_OFF_L */
				u8Data[3] = (uint8_t)((u16Off >> 8) & 0x0f);  /* REG_LEDx_OFF_H */
				/* Write ON/OFF registers using a multi-register write.
				 * This is necessary because the PCA9685 outputs will change
				 * at the end of an I2C transfer. So all registers should be
				 * written in one transfer, so that the channel doesn't glitch
				 * during writes. */
				PCA9685_vWriteRegisterMulti(REG_LEDx_ON_L + u8Channel * REG_LEDx_STRIDE, u8Data, sizeof(u8Data));
			}
		}
	}
	else /* Turn off */
	{
		for (i = 0; i < u8NumChannels; i++)
		{
			u8Channel = u8ChannelMap[u8Bulb * 3 + i];
			PCA9685_vWriteRegister(REG_LEDx_OFF_H + u8Channel * REG_LEDx_STRIDE, 0x10); // full OFF
			PCA9685_vWriteRegister(REG_LEDx_ON_H + u8Channel * REG_LEDx_STRIDE, 0x00); // disable full ON

		}
	}

}

/****************************************************************************
 *
 * NAME:       		PCA9685_vWriteRegister
 *
 * DESCRIPTION:		Writes to one of the PCA9685's registers
 *
 *
 * PARAMETERS:      Name     RW  Usage
 *         	        u8Reg    R   Register number to write to
 *         	        u8Data   R   Data to write to register
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void PCA9685_vWriteRegister(uint8 u8Reg, uint8 u8Data)
{
	vAHI_SiMasterWriteSlaveAddr(PCA9685_ADDRESS, FALSE);
	/* START, WRITE, ACK */
	bAHI_SiMasterSetCmdReg(TRUE, FALSE, FALSE, TRUE, TRUE, FALSE);
	while (bAHI_SiMasterPollTransferInProgress());
	vAHI_SiMasterWriteData8(u8Reg);
	/* WRITE, ACK */
	bAHI_SiMasterSetCmdReg(FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
	while (bAHI_SiMasterPollTransferInProgress());
	vAHI_SiMasterWriteData8(u8Data);
	/* STOP, WRITE, ACK */
	bAHI_SiMasterSetCmdReg(FALSE, TRUE, FALSE, TRUE, TRUE, FALSE);
	while (bAHI_SiMasterPollTransferInProgress());
}

/****************************************************************************
 *
 * NAME:       		PCA9685_vWriteRegisterMulti
 *
 * DESCRIPTION:		Writes to one or more of the PCA9685's registers
 *
 *
 * PARAMETERS:      Name     RW  Usage
 *         	        u8Reg    R   First register number to write to
 *         	        pu8Data  R   Pointer to array of register values
 *         	        u8Len    R   Number of registers to write to
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void PCA9685_vWriteRegisterMulti(uint8 u8Reg, uint8 *pu8Data, uint8 u8Len)
{
	uint8 i;

	vAHI_SiMasterWriteSlaveAddr(PCA9685_ADDRESS, FALSE);
	/* START, WRITE, ACK */
	bAHI_SiMasterSetCmdReg(TRUE, FALSE, FALSE, TRUE, TRUE, FALSE);
	while (bAHI_SiMasterPollTransferInProgress());
	vAHI_SiMasterWriteData8(u8Reg);
	/* WRITE, ACK */
	bAHI_SiMasterSetCmdReg(FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
	while (bAHI_SiMasterPollTransferInProgress());
	for (i = 0; i < u8Len; i++)
	{
		vAHI_SiMasterWriteData8(pu8Data[i]);
		if (i == (u8Len - 1))
		{
			/* Last byte */
			/* STOP, WRITE, ACK */
			bAHI_SiMasterSetCmdReg(FALSE, TRUE, FALSE, TRUE, TRUE, FALSE);
		}
		else
		{
			/* WRITE, ACK */
			bAHI_SiMasterSetCmdReg(FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
		}
		while (bAHI_SiMasterPollTransferInProgress());
	}
}
