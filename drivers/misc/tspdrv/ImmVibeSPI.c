#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>

#define ISA1000_BOARD_NAME		"ISA1000"
#define GPIO_ISA1000_EN			33
#define GPIO_HAPTIC_EN			50
#define ISA1000_VIB_DEFAULT_TIMEOUT	15000
#define PWM_CH_ID			1
#define PWM_FREQUENCY			25000

static struct pwm_device *pwm;
static unsigned int pwm_period_ns;

/* This function assumes an input of [-127, 127] and mapped to [1,255] */
/* If your PWM module does not take an input of [1,255] and mapping to [1%,99%]
**    Please modify accordingly
**/
static void isa1000_vib_set_level(int level)
{
	int rc = 0;

	if  (level != 0)
	{
		/* Set PWM duty cycle corresponding to the input 'level' */
		rc = pwm_config(pwm, (pwm_period_ns * (level + 128)) / 256, pwm_period_ns);
		if (rc < 0)
		{
			pr_err("%s: pwm_config fail\n", __func__);
			goto chip_dwn;
		}

		/* Enable the PWM output */
		rc = pwm_enable(pwm);
		if (rc < 0)
		{
			pr_err("%s: pwm_enable fail\n", __func__);
			goto chip_dwn;
		}

		/* Assert the GPIO_ISA1000_EN to enable ISA1000 */
		gpio_set_value_cansleep(GPIO_ISA1000_EN, 1);
	} else {
		/* Deassert the GPIO_ISA1000_EN to disable ISA1000 */
		gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);

		/* Disable the PWM output */
		pwm_disable(pwm);
	}

	return;

chip_dwn:
	gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);
}

static int isa1000_setup(void)
{
	int ret;
	struct regulator *vdd_regulator;

	ret = gpio_is_valid(GPIO_ISA1000_EN);
	if (!ret)
	{
		pr_err("%s: Invalid gpio %d\n", __func__, GPIO_ISA1000_EN);
		return ret;
	}

	ret = gpio_request(GPIO_ISA1000_EN, "gpio_isa1000_en");
	if (ret)
	{
		pr_err("%s: gpio %d request failed\n", __func__, GPIO_ISA1000_EN);
		return ret;
	}

	ret = gpio_is_valid(GPIO_HAPTIC_EN);
	if (!ret)
	{
		pr_err("%s: Invalid gpio %d\n", __func__, GPIO_ISA1000_EN);
		return ret;
	}

	ret = gpio_request(GPIO_HAPTIC_EN, "gpio_haptic_en");
	if (ret)
	{
		pr_err("%s: gpio %d request failed\n", __func__, GPIO_HAPTIC_EN);
		return ret;
	}

	gpio_direction_output(GPIO_ISA1000_EN, 0);
	gpio_direction_output(GPIO_HAPTIC_EN, 1);

	/* Request and reserve the PWM module for output TO ISA1000 */
	pwm = pwm_request(PWM_CH_ID, "isa1000");
	if (IS_ERR(pwm))
	{
		pr_err("%s: pwm request failed\n", __func__);
		return PTR_ERR(pwm);
	}

	vdd_regulator = regulator_get(NULL, "8226_l28");
	if (IS_ERR(vdd_regulator))
	{
		pr_err("%s:fail to get vdd l28\n", __func__);
		return PTR_ERR(vdd_regulator);
	}

	ret = regulator_enable(vdd_regulator);
	if (ret)
	{
		pr_err("Regulator vdd enable failed rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
** This function is necessary for the TSP Designer Bridge.
** Called to allocate the specified amount of space in the DIAG output buffer.
** OEM must review this function and verify if the proposed function is used in their software package.
*/
void* ImmVibeSPI_Diag_BufPktAlloc(int nLength)
{
	/* No need to be implemented */

	/* return allocate_a_buffer(nLength); */
	return NULL;
}

/*
** Called to disable amp (disable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	/* Disable amp */
	/* Disable the ISA1000 output */
	gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	/* Reset PWM frequency (only if necessary on Hong Mi 2A)*/
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% (which correspond to the output level 0) */
	isa1000_vib_set_level(0);

	/* Enable amp */
	/* Enable the ISA1000 output */
	gpio_set_value_cansleep(GPIO_ISA1000_EN, 1);

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	int ret;

	/* Kick start the ISA1000 set up */
	ret = isa1000_setup();
	if(ret < 0)
	{
		pr_err("%s, ISA1000 initialization failed\n", __func__);
		return VIBE_E_FAIL;
	}

	/* Set PWM duty cycle to 50%, i.e. output level to 0 */
	isa1000_vib_set_level(0);

	/* Compute the period of PWM cycle */
	pwm_period_ns = NSEC_PER_SEC / PWM_FREQUENCY;

	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Set PWM frequency (only if necessary on Hong Mi 2A) */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* i.e. output level to 0 */
	isa1000_vib_set_level(0);

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
	/*
	** For ERM:
	**	nBufferSizeInBytes should be equal to 1 if nOutputSignalBitDepth is equal to 8
	**	nBufferSizeInBytes should be equal to 2 if nOutputSignalBitDepth is equal to 16
	*/

	/* Below based on assumed 8 bit PWM, other implementation are possible */

	/* M = 1, N = 256, 1 <= nDutyCycle <= (N-M) */

	/* Output force: nForce is mapped from [-127, 127] to [1, 255] */
	int level;

	if(nOutputSignalBitDepth == 8)
	{
		if( nBufferSizeInBytes != 1)
		{
			pr_err("%s: Only support single sample for ERM\n",__func__);
			return VIBE_E_FAIL;
		}
		else
		{
			level = (signed char)(*pForceOutputBuffer);
	}
	}
	else if(nOutputSignalBitDepth == 16)
	{
		if( nBufferSizeInBytes != 2)
		{
			pr_err("%s: Only support single sample for ERM\n",__func__);
			return VIBE_E_FAIL;
		}
		else
		{
			level = (signed short)(*((signed short*)(pForceOutputBuffer)));
			/* Quantize it to 8-bit value as ISA1000 only support 8-bit levels */
			level >>= 8;
		}
	}
	else
	{
		pr_info("%s: Invalid Output Force Bit Depth\n",__func__);
		return VIBE_E_FAIL;
	}

	pr_debug("%s: level = %d\n", __func__, level);

	isa1000_vib_set_level(level);

	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	return VIBE_S_SUCCESS;
}

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/
VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname)
{
	return VIBE_S_SUCCESS;
}

/*
** Called to delete an IVT file
*/
VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname)
{
	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	if ((!szDevName) || (nSize < 1))
		return VIBE_E_FAIL;

	strncpy(szDevName, ISA1000_BOARD_NAME, nSize-1);
	szDevName[nSize - 1] = '\0';

	return VIBE_S_SUCCESS;
}
