#include "../Drivers/BSP/STM32F4-Discovery/stm32f4_discovery_audio.h"

#define AUDIO_BUFFER_SIZE             4096

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

extern __IO uint32_t RepeatState, PauseResumeStatus, PressCount;

/* Audio wave data length to be played */
static uint32_t WaveDataLength = 0;

/* Audio wave remaining data length to be played */
static __IO uint32_t AudioRemSize = 0;

__IO uint32_t AudioPlayStart = 0;

/* Ping-Pong buffer used for audio play */
uint8_t Audio_Buffer[AUDIO_BUFFER_SIZE];

/* Position in the audio play buffer */
__IO BUFFER_StateTypeDef buffer_offset = BUFFER_OFFSET_NONE;

/* Initial Volume level (from 0 (Mute) to 100 (Max)) */
static uint8_t Volume = 70;

/* Variable used by FatFs*/
FIL FileRead;
DIR Directory;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

// TODO @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ import discovery_audio
int WavePlayerInit(uint32_t AudioFreq)
{

  /* Initialize the Audio codec and all related peripherals (I2S, I2C, IOExpander, IOs...) */  
  return(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, Volume, AudioFreq));  
}


/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{ 
  buffer_offset = BUFFER_OFFSET_HALF;
}

/**
* @brief  Calculates the remaining file size and new position of the pointer.
* @param  None
* @retval None
*/
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  buffer_offset = BUFFER_OFFSET_FULL;
  BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE / 2);
}


/**
  * @brief  Plays Wave from a mass storage.
  * @param  AudioFreq: Audio Sampling Frequency
  * @retval None
*/
void WavePlayBack(uint32_t AudioFreq) {
    UINT bytesread = 0;
	
	/* Start playing */
	AudioPlayStart = 1;

    /* Initialize Wave player (Codec, DMA, I2C) */
    if(WavePlayerInit(AudioFreq) != 0) {
        Error_Handler();
    }

    /* Get Data from USB Flash Disk */
    f_lseek(&FileRead, 0);
    f_read (&FileRead, &Audio_Buffer[0], AUDIO_BUFFER_SIZE, &bytesread);
    AudioRemSize = WaveDataLength - bytesread;
	
    /* Start playing Wave */
    BSP_AUDIO_OUT_Play((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE);
	
    /* Check if the device is connected.*/
    while(AudioRemSize != 0) {
        bytesread = 0;
        if(buffer_offset == BUFFER_OFFSET_HALF) {
            f_read(&FileRead, &Audio_Buffer[0], AUDIO_BUFFER_SIZE/2, (void *)&bytesread);
            buffer_offset = BUFFER_OFFSET_NONE;
        }

        if(buffer_offset == BUFFER_OFFSET_FULL) {
            f_read(&FileRead, &Audio_Buffer[AUDIO_BUFFER_SIZE/2], AUDIO_BUFFER_SIZE/2, (void *)&bytesread);
            buffer_offset = BUFFER_OFFSET_NONE;
        }

        if(AudioRemSize > (AUDIO_BUFFER_SIZE / 2)) {
            AudioRemSize -= bytesread;
        } else {
            AudioRemSize = 0;
        }
		
		if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)) {	// checks if PA0 is set
			memset(Audio_Buffer, 0, AUDIO_BUFFER_SIZE);
			while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0));
			break;
		}
    }
    /* Stop playing Wave */
	AudioPlayStart = 0;
    WavePlayerStop();
    /* Close file */
    f_close(&FileRead);
    AudioRemSize = 0;
}

/**
  * @brief  Stops playing Wave.
  * @param  None
  * @retval None
  */
void WavePlayerStop(void) {
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
}

void WavePlayerStart(const char* path, char* wavefilename)
{
  UINT bytesread = 0;
  WAVE_FormatTypeDef waveformat;
  
  /* Get the read out protection status */
  if(f_opendir(&Directory, path) == FR_OK)
  {
    /* Open the Wave file to be played */
    if(f_open(&FileRead, wavefilename , FA_READ) == FR_OK)
    {    
      /* Read sizeof(WaveFormat) from the selected file */
      f_read (&FileRead, &waveformat, sizeof(waveformat), &bytesread);
      
      /* Set WaveDataLenght to the Speech Wave length */
      WaveDataLength = waveformat.FileSize;
    
      /* Play the Wave */
      WavePlayBack(waveformat.SampleRate);
    }    
  }
}
