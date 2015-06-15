#include <usb.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define    CC_CTRL         0xffc0
#define    AUX_ADDRESS     (0xFC00)  // Registers, address select
#define    AUX_DATA	   (0xFD00)  // Registers, write, data no address inc
#define    CSO_XDELAY2     0xee00
#define    C_ANAVAL0       0xe800
#define    C_ANAVAL2       0xea00
#define    C_ANASEL	   0xf000
#define    C_LAG0          0xe000	// 57344
#define    C_LRATE0	   0xf600
#define    WOI_YSTART      0x8000
#define    WOI_YEND        0x9000
#define    WOI_XSTART      0xa000
#define    WOI_XEND        0xb000
#define    WOI_YINC        0xc000
#define    WOI_XINC        0xd000
#define    CC_SHIFT_0BITS  0x00
#define	   CC_16BIT_8BIT   0x08	// To select 8 bit data or 16 bit (10 bit and 12 bit)
#define    CC_SINGLESHOT   0x80
#define    C_MISC          0xe700

#define MAXWIDTH    320
#define MAXHEIGHT   256

#define ADC_Vin_DEFAULT    2830  // original default was 2830
#define ADC_Vref_DEFAULT   2430  // original default was 2430
#define Vdet_comA_DEFAULT  3700  // original default was 3700
#define Vdet_comB_DEFAULT  3700  // original default was 3700

//  hroe: comments at end of each #define line are from original xenics code, unless
//        otherwise noted
#define CW9808_itr_DEFAULT               false  // Not relevant
// hroe: in original xenics code gain default was false, which is Low gain mode.  
//       We want high gain.
#define CW9808_gain_DEFAULT              true  
#define CW9808_multiplereadouts_DEFAULT  false  // Not relevant
#define CW9808_nondestructive_DEFAULT    false
#define CW9808_xinv_DEFAULT              false
#define CW9808_yinv_DEFAULT              false
#define CW9808_linerepeat_DEFAULT        false
#define CW9808_refout_DEFAULT            false
#define CW9808_reset_DEFAULT             false
#define CW9808_skim_DEFAULT              false
#define CW9808_power_DEFAULT             3  // Just use the highest values for these, range: 0 - 3
#define CW9808_current_DEFAULT           7  // 0 = 73uA, ..., 3 = 93uA,... , 7 = 147uA
#define CW9808_bias_DEFAULT              7  // 0 = 11nA, ..., 4 = 100 nA, ..., 7 = 167 nA
#define CW9808_bandwidth_DEFAULT         3  // range: 0-1-2-3
#define CW9808_outputfactor_DEFAULT      4  // Number of sensor outputs to use (1, 2 or 4)

enum XWriteRegisters1{
  Reg1_Cooling=0,      // 0 - Camera cooling
  Reg1_Continuous,     // 1 - Free running camera (Note: Use capture_data instead of CaptureSingle)
  Reg1_ExternalTrigger,// 2 - Activates/disables external triggering
  Reg1_Reserved_A,     // 3
  Reg1_Reserved_B,     // 4
  Reg1_Reserved_C,     // 5
  Reg1_Reserved_D,     // 6
  Reg1_InvertImage     // 7 - Inverts the image in the camera hardware
};

enum XevaRequest{
  X_Anchor     	= 0xA0,		// Native chip support		(ezusb)
  X_WriteI2C    = 0xA6,		// 64 byte blocks		(C_WRI2C)
  X_ReadI2C	= 0xA7,		// 64 byte blocks		(C_RDI2C)
  X_StartTTB   	= 0xA4,		// no parms		   (C_CONF_SIGNAL)
  X_Data       	= 0xA5,		// 64 byte blocks		(C_DATA)
  X_CmdFPGA    	= 0xA8,		// FD FF	    		(C_FPGA_CMD)
  X_StatFPGA   	= 0xA9,		// Get fpga status
  X_ResetEP     = 0xAA,		// no parms	    	(C_USB_RESET_EP)
  X_ControlM	= 0xAB,		// Start/stop cc command logging in SDF
  X_SDF_Write	= 0xB0,		// ?
  X_SDF_Read	= 0xB1,		// ?
  X_SDF_Data	= 0xB2		// ?
};

enum XWriteRegisters32{ Reg32_SensorWord = 0 };

enum XWriteRegisters16{
  Reg16_ADC_Vin=0,			// Control bits
  Reg16_ADC_Vref,		
  Reg16_Vdet_com,		
  Reg16_Vdet_com2,		
  Reg16_PWM,
  Reg16_PWM2,
  Reg16_P,				// X-Delay 0
  Reg16_I,				
  Reg16_D,				// X-Delay 1
  Reg16_SETTLE,
  Reg16_SensorVIN,		// Anasel 0 -> MCT camera registers
  Reg16_SensorVREF,
  Reg16_ADC_VinX,			// Anasel 1
  Reg16_ADC_VrefX,
  Reg16_PWMX,				// Anasel 2
  Reg16_ReservedA
};

enum XReadRegisters16{
  Reg16_Temperature  = 0, // Temperature circuit, ADU value
  Reg16_TempType	   = 3, // Type of temperature circuit
  Reg16_AAAA		   = 5, // Register used to detect read register capabilities. Should read 0xAAAA if these read registers are present.
  Reg16_HeadType	   = 6, 
  Reg16_LogicVersion = 7
};

enum XCCERRORs  {
    XCC_I_OK			= 0,
    XCC_E_TimeOut		= (1 <<  0),
    XCC_E_Underrun		= (1 <<  1),
    XCC_E_NotFound		= (1 <<  2),
    XCC_E_NoSuchReg		= (1 <<  3),
    XCC_E_NotSupported          = (1 <<  4),
    XCC_E_Undef_5		= (1 <<  5),
    XCC_E_Undef_6		= (1 <<  6),
    XCC_E_Undef_7		= (1 <<  7),
    XCC_E_Undef_8		= (1 <<  8),
    XCC_E_Undef_9		= (1 <<	 9),
    XCC_E_Undef_10		= (1 << 10),
    XCC_E_Undef_11		= (1 << 11),
    XCC_E_Undef_12		= (1 << 12),
    XCC_E_Undef_13		= (1 << 13),
    XCC_E_Undef_14		= (1 << 14),
    XCC_E_Undef_15		= (1 << 15),
    XCC_E_Undef_16		= (1 << 16),
    XCC_E_Undef_17		= (1 << 17),
    XCC_E_Undef_18		= (1 << 18),
    XCC_E_Undef_19		= (1 << 19),
    XCC_E_Undef_20		= (1 << 20),
    XCC_E_Undef_21		= (1 << 21),
    XCC_E_Undef_22		= (1 << 22),
    XCC_E_Undef_23		= (1 << 23),
    XCC_E_Undef_24		= (1 << 24),
    XCC_E_Undef_25		= (1 << 25),
    XCC_E_Undef_26		= (1 << 26),
    XCC_E_Undef_27		= (1 << 27),
    XCC_E_Undef_28		= (1 << 28),
    XCC_E_Undef_29		= (1 << 29),
    XCC_E_Undef_30		= (1 << 30),
    XCC_E			= (1 << 31)
};

// TODO: create a function to read out type of XCCERROR

typedef int XCCERROR;

typedef struct {
        int        camera_found_on_usb;
        int        curPWM;
        int        curFan;
        int        curIntegrationTimeMillisec;
        int        cur_ADC_Vin;
        int        cur_ADC_Vref;
        int        cur_Vdet_comA;
        int        cur_Vdet_comB;
        bool       cur_itr;
        bool       cur_gain;
        bool       cur_multiplereadouts;
        bool       cur_nondestructive;
        bool       cur_xinv;
        bool       cur_yinv;
        bool       cur_linerepeat;
        bool       cur_refout;
        bool       cur_reset;
        bool       cur_skim;
        int        cur_power;
        int        cur_current;
        int        cur_bias;
        int        cur_bandwidth;
        int        cur_outputfactor;
        unsigned short                  CameraID;
        int                             image_capture_timeout;
        int                             command_timeout;
        usb_dev_handle	               *m_hDevice;
} XCCHANDLE_I;

XCCHANDLE_I *hnd;

long FileLength(FILE *fp)
{
  long len=0,lpos=0;
  lpos = ftell(fp);
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  fseek(fp, lpos, SEEK_SET);
  return(len);
}

int get_max_width() { return(MAXWIDTH); }
int get_max_height() { return(MAXHEIGHT); }
int get_camera_found_on_usb() { return (*hnd).camera_found_on_usb; }
int get_image_capture_timeout() { return (*hnd).image_capture_timeout; }
int get_command_timeout() { return (*hnd).command_timeout; }

// TODO 2014-01-10: examine how this timeout is actually used
int calculate_timeout(int integrationTimeMillisec) { return 5000 + (integrationTimeMillisec); }

XCCERROR convert_usb_error(const char *location, int error, int sent)
{
  if(sent == error) return(XCC_I_OK);
  fprintf(stderr,"xenics.convert_usb_error Error: %s=%d (%s)\n",location,error,usb_strerror());
  return(XCC_E);
}

XCCERROR get_status(XCCHANDLE_I *hnd, unsigned short *value, 
                         unsigned short *eepromsize)
{
  int retUSB = XCC_I_OK;
  int retXCC = 0;
  typedef struct {
    unsigned char valid;			// EP0BUF[0] = 1
    unsigned char eepromtype;		//           = I2CS in firmware
    unsigned char unk1;
    unsigned char unk2;
    unsigned char unk3;
    unsigned char unk4;
    unsigned char unk5;
    unsigned char unk6;
    unsigned short temperature;		// EP0BUF[8] = temp low
                                        // EP0BUF[9] = temp hi
    char filler[54];		// Make sure the packet is 64 bytes.
  } USBSTATUS;
  USBSTATUS status;
  memset(&status,0,10);
  retUSB  = usb_control_msg((*hnd).m_hDevice, 
                 USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
                 X_StatFPGA, 0, 0, (char*) &status, sizeof(USBSTATUS), 
                 (*hnd).command_timeout);
  retXCC |= convert_usb_error("get_status", retUSB, (retUSB > 0 ? retUSB : sizeof(USBSTATUS)) );
  if(value)
    *value = status.temperature;
  if(eepromsize)
    switch( (status.eepromtype>>3)&3 )
      {
      case 1: // 4k eeprom
	*eepromsize = 0x1000;
	break;
      case 2:	// 8k eeprom
	*eepromsize = 0x2000;
	break;
      case 3: // 16k eeprom
	*eepromsize = 0x4000;
	break;
      default:
	*eepromsize = 0;
      }
  return( retXCC );
}

XCCERROR send_command_to_FPGA(unsigned short value)
{
  XCCERROR retXCC = XCC_I_OK;
  int bytes = 2;
  int retUSB = 0;
  retUSB = usb_control_msg((*hnd).m_hDevice,
                USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                X_CmdFPGA, 0, 0, (char*)(&value), bytes,
                (*hnd).command_timeout);
  retXCC |= convert_usb_error("XCC_Flush", retUSB, bytes);
  return(retXCC);
}

XCCERROR set_generic_long(unsigned short base, unsigned long value)
{
  XCCERROR ret = XCC_I_OK;
  unsigned char b0,b1,b2,b3;
  b0=(unsigned char)(   value & 0xFF				);
  b1=(unsigned char)( ( value & 0xFF00)     >>  8 );
  b2=(unsigned char)( ( value & 0xFF0000)   >> 16	);
  b3=(unsigned char)( ( value & 0xFF000000) >> 24	);
  ret |= send_command_to_FPGA(base+0x0300+b3);
  ret |= send_command_to_FPGA(base+0x0200+b2);
  ret |= send_command_to_FPGA(base+0x0100+b1);
  ret |= send_command_to_FPGA(base+0x0000+b0);	
  return(ret);
}

XCCERROR set_generic_short(unsigned short base, unsigned short value)
{
  XCCERROR ret = XCC_I_OK;
  unsigned char b0,b1;
  b0=(unsigned char)(   value & 0xFF				);
  b1=(unsigned char)( ( value & 0xFF00)     >>  8 );
  ret |= send_command_to_FPGA(base+0x0000+b0);
  ret |= send_command_to_FPGA(base+0x0100+b1);
  return(ret);
}

XCCERROR get_register16(unsigned int reg, unsigned short *value)
{
  XCCERROR ret = XCC_I_OK;
  ret |= send_command_to_FPGA(AUX_ADDRESS | 255);	
  ret |= send_command_to_FPGA(AUX_DATA | (reg & 0xff));
  get_status(hnd, value, NULL);
  ret |= send_command_to_FPGA(AUX_ADDRESS | 255);	
  ret |= send_command_to_FPGA(AUX_DATA | 0);
  return(ret);
}

XCCERROR set_register16(unsigned int reg, unsigned short value)
{
  XCCERROR ret = XCC_I_OK;
  unsigned short tempvalue;
  switch(reg)
    {
    case Reg16_ADC_Vin:	
    case Reg16_ADC_Vref: 
    case Reg16_Vdet_com: 
    case Reg16_Vdet_com2:
    case Reg16_PWM:	
    case Reg16_PWM2:	
      {
	tempvalue = (4 << 8) | (   ((reg-Reg16_ADC_Vin)/2)&1      );
	ret |= send_command_to_FPGA( CC_CTRL | ( (tempvalue >> 7) | (tempvalue & 0x01) ) );
	tempvalue = (5 << 8) | ( ( ((reg-Reg16_ADC_Vin)/2)&2) >>1 );
	ret |= send_command_to_FPGA( CC_CTRL | ( (tempvalue >> 7) | (tempvalue & 0x01) ) );
	if((reg-Reg16_ADC_Vin)%2 == 0)
	  ret |= set_generic_short(C_ANAVAL0, value);
	else
	  ret |= set_generic_short(C_ANAVAL2, value);
	tempvalue = (4 << 8) | 1;
	ret |= send_command_to_FPGA(CC_CTRL | ( (value >> 7) | (value & 0x01) ) ); 
	tempvalue = (5 << 8) | 1;
	ret |= send_command_to_FPGA(CC_CTRL | ( (value >> 7) | (value & 0x01) ) );  
      }
      break;
    case Reg16_P:			
    case Reg16_I:			
    case Reg16_D:			
    case Reg16_SETTLE:
      {
	ret |= send_command_to_FPGA(CSO_XDELAY2 | (((reg-Reg16_P) / 2) & 0xff));		
	if((reg-Reg16_P)%2 == 0)
	  ret |= set_generic_short(C_ANAVAL0, value);
	else
	  ret |= set_generic_short(C_ANAVAL2, value);
	if (reg==Reg16_D||reg==Reg16_SETTLE)
	  ret |= send_command_to_FPGA(CSO_XDELAY2 | (255 & 0xff));
      }
      break;
    case Reg16_ADC_VinX:
    case Reg16_ADC_VrefX:
    case Reg16_PWMX:
    case Reg16_ReservedA:
      {
	ret |= send_command_to_FPGA(C_ANASEL | (((reg - Reg16_ADC_VinX) / 2) & 0xff));
	if((reg-Reg16_ADC_VinX)%2 == 0)
	  ret |= set_generic_short(C_ANAVAL0, value);
	else
	  ret |= set_generic_short(C_ANAVAL2, value);
	ret |= send_command_to_FPGA(C_ANASEL | (255 & 0xff));
      }
      break;
    default:
      ret = XCC_E_NoSuchReg;
      break;
    }
  return(ret);
}

XCCERROR set_register32(unsigned int reg, unsigned long value)
{
  XCCERROR ret = XCC_I_OK;
  unsigned short tempvalue;
  switch(reg)
    {
    case Reg32_SensorWord:
      tempvalue = (6 << 8) | 0;
      ret |= send_command_to_FPGA(CC_CTRL | ( (tempvalue >> 7) | 
                                (tempvalue & 0x01) ) ); 
      tempvalue = (3 << 8) | 0;
      ret |= send_command_to_FPGA(CC_CTRL | ( (tempvalue >> 7) | 
                                (tempvalue & 0x01) ) );  
      ret |= set_generic_long(C_LRATE0, value);  
      break;
    default:
      ret = XCC_E_NoSuchReg;
      break;
    }
  return(ret);
}

int set_ADC_Vin(int ADC_Vin)
{
  XCCERROR xcc_ret = XCC_I_OK;
  xcc_ret |= set_register16(Reg16_ADC_Vin, ADC_Vin);
  (*hnd).cur_ADC_Vin = ADC_Vin;
  return (int)xcc_ret;
}

int set_ADC_Vref(int ADC_Vref)
{
  XCCERROR xcc_ret = XCC_I_OK;
  xcc_ret |= set_register16(Reg16_ADC_Vref, ADC_Vref);
  (*hnd).cur_ADC_Vref = ADC_Vref;
  return (int)xcc_ret;
}

int set_Vdet_comA(int Vdet_comA)
{
  XCCERROR xcc_ret = XCC_I_OK;
  xcc_ret |= set_register16(Reg16_Vdet_com, Vdet_comA);
  (*hnd).cur_Vdet_comA = Vdet_comA;
  return (int)xcc_ret;
}

int set_Vdet_comB(int Vdet_comB)
{
  XCCERROR xcc_ret = XCC_I_OK;
  xcc_ret |= set_register16(Reg16_Vdet_com2, Vdet_comB);
  (*hnd).cur_Vdet_comB = Vdet_comB;
  return (int)xcc_ret;
}

int get_ADC_Vin(){ return (*hnd).cur_ADC_Vin; }
int get_ADC_Vref(){ return (*hnd).cur_ADC_Vref; }
int get_Vdet_comA(){ return (*hnd).cur_Vdet_comA; }
int get_Vdet_comB(){ return (*hnd).cur_Vdet_comB; }

int set_fan(int FanState)
{ // FanState = 0 turns fan Off anything else turns fan On
  XCCERROR xcc_ret = XCC_I_OK;
  unsigned short tempvalue;
  tempvalue = ((Reg1_Cooling & 0xff) << 8) | (FanState ? 1:0);  
  xcc_ret |= send_command_to_FPGA(CC_CTRL | 
                             ( (tempvalue >> 7) | (tempvalue & 0x01) ) );
  (*hnd).curFan = (FanState ? 1:0);
  return((int)xcc_ret);
}

int get_fan() { return (*hnd).curFan; }

int set_pwm(int usPWM)
{
  XCCERROR xcc_ret = XCC_I_OK;
  int maxPWM = 4095;  
  int limitedPWM;
  limitedPWM = usPWM;
  if (usPWM < 0)  limitedPWM = 0;
  if (usPWM > maxPWM)  limitedPWM = maxPWM;  
  (*hnd).curPWM = limitedPWM;
  xcc_ret |= send_command_to_FPGA(C_ANASEL | (255 & 0xff));  
  xcc_ret |= send_command_to_FPGA(0xffc9);
  xcc_ret |= send_command_to_FPGA(0xffcb);
  xcc_ret |= send_command_to_FPGA(CSO_XDELAY2 | (0 & 0xff));
  xcc_ret |= set_generic_short(C_ANAVAL0, 0);
  xcc_ret |= send_command_to_FPGA(CSO_XDELAY2 | (0 & 0xff));
  xcc_ret |= set_generic_short(C_ANAVAL2, 0);
  xcc_ret |= send_command_to_FPGA(CSO_XDELAY2 | (1 & 0xff));
  xcc_ret |= set_generic_short(C_ANAVAL0, 0);
  xcc_ret |= send_command_to_FPGA(CSO_XDELAY2 | (1 & 0xff));
  xcc_ret |= set_generic_short(C_ANAVAL2, 0);
  xcc_ret |= send_command_to_FPGA(CSO_XDELAY2 | (255 & 0xff));
  xcc_ret |= send_command_to_FPGA(0xffc8);
  xcc_ret |= send_command_to_FPGA(0xffcb);
  xcc_ret |= set_generic_short(C_ANAVAL0, limitedPWM);
  xcc_ret |= send_command_to_FPGA(0xffc9);
  xcc_ret |= send_command_to_FPGA(0xffcb);
  return (int)xcc_ret;
}


int get_pwm() { return (*hnd).curPWM; }


// TODO 2014-01-10: figure out if get_temperature_ADCtype (formerly GetTemperature_ADCtype) is useful - we hadn't been using it in the old server
int get_temperature_ADCtype()
{
  XCCERROR xcc_ret = XCC_I_OK;
  unsigned short ADCtype = 1;
  xcc_ret |= get_register16(Reg16_TempType, &ADCtype);
  return (int)(ADCtype);
}


int get_temperature_ADU()
{
  XCCERROR xcc_ret = XCC_I_OK;
  unsigned short adu = 0;
  xcc_ret |= get_register16(Reg16_Temperature, &adu);
  return (int)(adu);
}


void load_buffer(const  char *buffer,unsigned long length)
{
  int dest = 0; 
  unsigned int i;
  usb_control_msg((*hnd).m_hDevice, 
		  USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
		  X_StartTTB, dest, 0, 0, 0, (*hnd).command_timeout); //);
  for(i=0;i<length/64;i++)
    {
      char *buf = (char*) (buffer + (i*64));
      usb_control_msg((*hnd).m_hDevice, 
		      USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
		      X_Data, 0, 0, buf, 64, (*hnd).command_timeout); 
    }
  if(length % 64 != 0)
    {
      char *buf = (char*) (buffer + ((length/64)*64));
      int remlen = length % 64;
      usb_control_msg((*hnd).m_hDevice, 
		      USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
		      X_Data, 0, 0, buf, remlen, (*hnd).command_timeout); 
    }
  usb_control_msg((*hnd).m_hDevice, 
		  USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
		  X_CmdFPGA, 0, 0, (char*)&"\xfd\xff", 2, 
                  (*hnd).command_timeout); 
  usb_control_msg((*hnd).m_hDevice, 
		  USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, 
		  X_ResetEP, 0, 0, 0, 0, (*hnd).command_timeout); 
}


void load_buffer_file(const char *filename)
{
  FILE *fp=fopen(filename, "rb");
  char *buffer = NULL;
  size_t  length  = FileLength(fp);
  buffer = (char*) calloc(sizeof(char), length);
  if(fread(buffer, sizeof(char), length, fp) == length)
    load_buffer(buffer, length);
}


XCCERROR set_WOI(int xs, int ys, int xe, int ye)
{
  XCCERROR ret = XCC_I_OK;
  ret |= send_command_to_FPGA(WOI_YSTART | (ys & 0x0fff));
  ret |= send_command_to_FPGA(WOI_YEND   | (ye & 0x0fff));
  ret |= send_command_to_FPGA(WOI_XSTART | (xs & 0x0fff));
  ret |= send_command_to_FPGA(WOI_XEND   | (xe & 0x0fff));
  ret |= send_command_to_FPGA(WOI_YINC   | (1 & 0x0f));
  ret |= send_command_to_FPGA(WOI_XINC   | (1 & 0x0f));	
  return(ret);
}


XCCERROR set_frame_rate(unsigned short hz, unsigned int width, unsigned int height)
{
  unsigned int framerate = 0;
  unsigned int cliprate = 0;
  //hroe:  is 350 really our max frame rate?? I would have thought it was 100??
  framerate = 350; // 350 hz max
  cliprate = 0;
  if (hz > framerate)
    hz = framerate;
  bool	bUseDelay = true;
  // Compensate for window of interest.
  double fMaxRate_Real = (177715.0 * pow(width*height, -0.6362)) * 
                         (framerate / 120.0);
  if(hz >= fMaxRate_Real)
    bUseDelay = false;
  float delayf = (1.0/(double)hz) * 1000 * 1000;
  int   delay  = delayf /*- 2583*/;
  if(delay < 0)
    delay = 0;
  if(delay>65535)
    delay = 65535;
  unsigned int adHighByte = 192;
  unsigned int adLowByte	= 191;
  unsigned int adControl	= 190;
  if(bUseDelay)
    {
      send_command_to_FPGA(AUX_ADDRESS | adHighByte);	// Register select
      send_command_to_FPGA(AUX_DATA    |	((delay>>8) & 0xff) );
      send_command_to_FPGA(AUX_ADDRESS | adLowByte );
      send_command_to_FPGA(AUX_DATA    |	((delay   ) & 0xff) );
    }
  send_command_to_FPGA(AUX_ADDRESS | adControl);         // control register
  send_command_to_FPGA(AUX_DATA    | (bUseDelay?1:0) );  // turn delay on/off
  return XCC_I_OK;
}


// +------------------- Singleshot mode (altijd aan)
// |+------------------ Diagnostic               = X-Diag
// ||+----------------- Combined with diagnostic = Y-Diag
// |||+---------------- Combined with diagnostic and y-diag = XOR diag
// |||| +-------------- 16 bit mode
// |||| |
// 0123 4567
XCCERROR set_image_source_to_camera()
{ // We don't want any bit shifting.
  unsigned short val = CC_SHIFT_0BITS | CC_16BIT_8BIT | CC_SINGLESHOT ;
  return(send_command_to_FPGA(C_MISC + val ));
}


XCCERROR set_capture_mode()
{
  XCCERROR xcc_ret = XCC_I_OK;
  unsigned short tempvalue;
  tempvalue = ((Reg1_Continuous & 0xff) << 8) | (1 ? 1:0);
  xcc_ret |= send_command_to_FPGA(CC_CTRL | ( (tempvalue >> 7) | 
                           (tempvalue & 0x01) ) );
  tempvalue = ((Reg1_ExternalTrigger & 0xff) << 8) | (0 ? 1:0);
  xcc_ret |= send_command_to_FPGA(CC_CTRL | ( (tempvalue >> 7) | 
                           (tempvalue & 0x01) ) );
  return xcc_ret;
}


int capture_data(char *buffer, int caplen)
{
  int retUSB = 0;
  int retXCC = XCC_I_OK;
  retUSB = usb_bulk_read((*hnd).m_hDevice, 0x82, buffer, caplen, 
                         (*hnd).image_capture_timeout * 2);
  if(retUSB != caplen) 
    {
      fprintf(stderr,"capture_data Error: usb_bulk_read returns %d (%s)",
              retUSB, usb_strerror());
      fprintf(stderr,
      "capture_data Error: usb_bulk_read returns %d (%s), errno = %d\n", 
	      retUSB, usb_strerror(), errno);
      fprintf(stderr,
           "capture_data usb_bulk_read timeout was: %d, caplen: %d\n", 
	      (*hnd).image_capture_timeout * 2, caplen);
      if(retUSB >0 && retUSB < caplen)
	  retXCC = XCC_E_Underrun;
      else
	{
	  switch(retUSB)
	    {
	    case -110:						// Timeout
	    case -116:						// Timeout
	      retXCC = XCC_E_TimeOut;
	      break;
	    case -5:    // A device attached to the system is not functioning
	      retXCC = XCC_E;
	      break;
	    default:
	      retXCC = XCC_E;
	      break;
	    }
	}
    }
  return((int)retXCC);
}


//TODO 2014-01-10: looks like long exposure sequences are handled entirely within this.  
//                 If so, need to monitor for some sort of abort signal?
void capture_frames(unsigned short *FrameBuffer, int n_pix)
{
  XCCERROR xccerr;
  unsigned int singleFrameSizeWords = MAXWIDTH*MAXHEIGHT;
  unsigned int singleFrameSizeBytes;
  unsigned int fullBufferSizeWords,fullBufferSizeBytes;
  unsigned int i;
  unsigned int n_frames = ((unsigned int)n_pix) / singleFrameSizeWords;
  singleFrameSizeBytes = singleFrameSizeWords*2;
  fullBufferSizeWords = singleFrameSizeWords*n_frames;
  fullBufferSizeBytes = fullBufferSizeWords*2;
//   fprintf(stdout,"capture_frames:\n");
  for(i=0;i<n_frames;i++)
    {
      xccerr = (XCCERRORs)capture_data((char*) (FrameBuffer + i*singleFrameSizeWords), singleFrameSizeBytes);
      if(xccerr != XCC_I_OK)
        fprintf(stdout,"capture_data NOT OK with error = %i\n",xccerr);
    }
}


void take_dummy_frame()
{
  unsigned int xBufSizWords,xBufSizBytes;
  xBufSizWords = MAXWIDTH*MAXHEIGHT;
  xBufSizBytes = xBufSizWords*2;
  unsigned short *FrameBuffer = (unsigned short *)malloc(xBufSizBytes); 
  //  2012-02-28: for reasons unclear to me (hroe) I am needing to take 2 dummy frames
  capture_data((char*) FrameBuffer, xBufSizBytes);
  capture_data((char*) FrameBuffer, xBufSizBytes);
}

// TODO: 2014-01-13:  combine _set_commandword9808 & set_commandword9808 into single subroutine

XCCERROR _set_commandword9808(
          bool itr,              // Not relevant
          bool gain,             // < Low gain mode
          bool multiplereadouts, // Not relevant \/
          bool nondestructive,
          bool xinv,
          bool yinv,
          bool linerepeat,
          bool refout,
          bool reset,
          bool skim,
          int power, // Just use the highest values for these, range: 0 - 3
          int current,       // 0 = 73uA, ..., 3 = 93uA,... , 7 = 147uA
          int bias,          // 0 = 11nA, ..., 4 = 100 nA, ..., 7 = 167 nA
          int bandwidth,     // range: 0-1-2-3
          int outputfactor)  // Number of sensor outputs to use (1, 2 or 4)
{
  XCCERROR xcc_ret = XCC_I_OK;
  unsigned long cmd = 0x80000000;
  if(itr) cmd |= 0x20000000;
  if(gain) cmd |= 0x10000000;	// on-off (true-false)
  if(multiplereadouts) cmd |= 0x20000;
  if(nondestructive!=0) cmd |= 0x10000;
  if(!xinv) cmd |= 0x80; // Swapped compared to 9702 for backwards compatible
  if(!yinv) cmd |= 0x40;
  if(linerepeat) cmd |= 0x20;
  if(refout) cmd |= 0x4;
  if(reset) cmd |= 0x2;
  if(skim) cmd |= 0x1;
  cmd |= ((power & 3) << 26);	// 2b
  cmd |= ((current & 7) << 23);	// 3b
  cmd |= ((bias	& 7) << 20);	// 3b
  cmd |= ((bandwidth & 3) << 18);	// 2b
  switch (outputfactor)
    {
    default:
    case 1:
      cmd |=((0) <<3);
      break;
    case 2:
      cmd |=((2) <<3);
      break;
    case 4:
      cmd |=((3) <<3);
      break;
    }
  xcc_ret |= set_register32(Reg32_SensorWord, cmd);
  xcc_ret |= set_register32(Reg32_SensorWord, cmd);
  unsigned short select3 = 0;
  unsigned short select4 = 0;
  //hroe: we are hardwiring that hardwarePixelCorrection = False
  select3 = (0xffc0 | ( ( (( (6 << 8) | 1) >> 7) | ((6 << 8) | 1) ) & 0x01));
  select4 = (0xffc0 | ( ( (( (3 << 8) | 0) >> 7) | ((3 << 8) | 0) ) & 0x01));
  xcc_ret |= send_command_to_FPGA(select3);
  xcc_ret |= send_command_to_FPGA(select4);
  return xcc_ret;
}


XCCERROR set_commandword9808()
{ return _set_commandword9808((*hnd).cur_itr,(*hnd).cur_gain,
         (*hnd).cur_multiplereadouts,(*hnd).cur_nondestructive,
         (*hnd).cur_xinv,(*hnd).cur_yinv,(*hnd).cur_linerepeat,
         (*hnd).cur_refout,(*hnd).cur_reset,(*hnd).cur_skim,
         (*hnd).cur_power,(*hnd).cur_current,(*hnd).cur_bias,
         (*hnd).cur_bandwidth,(*hnd).cur_outputfactor); }


void _set_integration_time_millisec(unsigned long iTime)
{
  XCCERROR xcc_ret = XCC_I_OK;
  // expect iTime in millisec, but xenics requires microseconds  
  xcc_ret |= set_generic_long(C_LAG0,
                                  (unsigned long) (iTime * 1000 * 40));
  // I think the  * 40 relates to a 40mhz clock.
  (*hnd).image_capture_timeout = calculate_timeout((int)iTime);
  (*hnd).curIntegrationTimeMillisec = (int)(iTime);
}

void set_integration_time_millisec(unsigned long iTime)
{ // reason to have _Set... separately is so that one can call it withotu
//having a dummy frame taken when in the midst of camera startup, otherwise
// we see timeouts during startup
  _set_integration_time_millisec(iTime);
  take_dummy_frame();  
}


int get_integration_time_millisec()
{ return (*hnd).curIntegrationTimeMillisec; }

// HEREIAM adding to xenics.i

XCCERROR set_gain(int gain)
{
  XCCERROR xcc_ret;
  (*hnd).cur_gain = (gain ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 


XCCERROR set_nondestructive(int nondestructive)
{
  XCCERROR xcc_ret;
  (*hnd).cur_nondestructive = (nondestructive ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_xinv(int xinv)
{
  XCCERROR xcc_ret;
  (*hnd).cur_xinv = (xinv ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_yinv(int yinv)
{
  XCCERROR xcc_ret;
  (*hnd).cur_yinv = (yinv ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_linerepeat(int linerepeat)
{
  XCCERROR xcc_ret;
  (*hnd).cur_linerepeat = (linerepeat ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_refout(int refout)
{
  XCCERROR xcc_ret;
  (*hnd).cur_refout = (refout ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_reset(int reset)
{
  XCCERROR xcc_ret;
  (*hnd).cur_reset = (reset ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_skim(int skim)
{
  XCCERROR xcc_ret;
  (*hnd).cur_skim = (skim ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_power(int power)
{
  XCCERROR xcc_ret;
  (*hnd).cur_power = (power ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_current(int current)
{
  XCCERROR xcc_ret;
  (*hnd).cur_current = (current ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_bias(int bias)
{
  XCCERROR xcc_ret;
  (*hnd).cur_bias = (bias ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_bandwidth(int bandwidth)
{
  XCCERROR xcc_ret;
  (*hnd).cur_bandwidth = (bandwidth ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_outputfactor(int outputfactor)
{
  XCCERROR xcc_ret;
  (*hnd).cur_outputfactor = (outputfactor ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_itr(int itr)
{
  XCCERROR xcc_ret;
  (*hnd).cur_itr = (itr ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

XCCERROR set_multiplereadouts(int multiplereadouts)
{
  XCCERROR xcc_ret;
  (*hnd).cur_multiplereadouts = (multiplereadouts ? 1:0);
  xcc_ret = set_commandword9808();
  take_dummy_frame();
  return (int)xcc_ret;
} 

int get_gain() { return (*hnd).cur_gain; }
int get_nondestructive() { return (*hnd).cur_nondestructive; }
int get_xinv() { return (*hnd).cur_xinv; }
int get_yinv() { return (*hnd).cur_yinv; }
int get_linerepeat() { return (*hnd).cur_linerepeat; }
int get_refout() { return (*hnd).cur_refout; }
int get_reset() { return (*hnd).cur_reset; }
int get_skim() { return (*hnd).cur_skim; }
int get_power() { return (*hnd).cur_power; }
int get_current() { return (*hnd).cur_current; }
int get_bias() { return (*hnd).cur_bias; }
int get_bandwidth() { return (*hnd).cur_bandwidth; }
int get_outputfactor() { return (*hnd).cur_outputfactor; }
int get_itr() { return (*hnd).cur_itr; }
int get_multiplereadouts() { return (*hnd).cur_multiplereadouts; }

void set_9808_params_to_default()
{
  (*hnd).cur_itr = CW9808_itr_DEFAULT;
  (*hnd).cur_gain = CW9808_gain_DEFAULT;
  (*hnd).cur_multiplereadouts = CW9808_multiplereadouts_DEFAULT;
  (*hnd).cur_nondestructive = CW9808_nondestructive_DEFAULT;
  (*hnd).cur_xinv = CW9808_xinv_DEFAULT;
  (*hnd).cur_yinv = CW9808_yinv_DEFAULT;
  (*hnd).cur_linerepeat = CW9808_linerepeat_DEFAULT;
  (*hnd).cur_refout = CW9808_refout_DEFAULT;
  (*hnd).cur_reset = CW9808_reset_DEFAULT;
  (*hnd).cur_skim = CW9808_skim_DEFAULT;
  (*hnd).cur_power = CW9808_power_DEFAULT;
  (*hnd).cur_current = CW9808_current_DEFAULT;
  (*hnd).cur_bias = CW9808_bias_DEFAULT;
  (*hnd).cur_bandwidth = CW9808_bandwidth_DEFAULT;
  (*hnd).cur_outputfactor = CW9808_outputfactor_DEFAULT;
}

void open_camera()
{
  hnd = (XCCHANDLE_I *) malloc( sizeof(XCCHANDLE_I) );
  XCCERROR xcc_ret = XCC_I_OK;
  struct usb_bus *bus;
  int ret;
  struct usb_device *curdev,*camera_dev=NULL;
  usb_dev_handle *udev;
  (*hnd).camera_found_on_usb = 0;
  (*hnd).m_hDevice		   	= NULL;
  (*hnd).image_capture_timeout	= 0x00ffffff; 
  (*hnd).command_timeout	= 0x00ffffff; 
  set_9808_params_to_default();
  usb_init();
  usb_find_busses();
  usb_find_devices();
  for(bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (curdev = bus->devices; curdev; curdev = curdev->next) {
            fprintf(stderr,"bus,vendor,product:  %s  %x  %x",
                bus->dirname,curdev->descriptor.idVendor, 
                curdev->descriptor.idProduct);
            if(curdev->descriptor.idVendor == 0x1b21 &&
               curdev->descriptor.idProduct == 0x8637)
              {
                fprintf(stderr,"   <----  there's our camera!\n");
                camera_dev = curdev;
                (*hnd).CameraID = curdev->descriptor.idProduct;
                    (*hnd).camera_found_on_usb = 1;
              }
            else if(curdev->descriptor.idVendor == 0x1b21)
              fprintf(stderr,"   <----  found an unrecognized XENICS device\n");
            else
              fprintf(stderr,"\n");
        }
    }
  if((*hnd).camera_found_on_usb == 0)  {
    fprintf(stderr, "camera not found on USB\n");
    return;
  }
  udev = usb_open(camera_dev);
  if( 0 != (ret = usb_set_configuration(udev, camera_dev->config[0].bConfigurationValue)) )
    fprintf(stderr,"SetupCameraHandle Error:  usb_set_configuration returns %d (%s)", ret, usb_strerror());
  if( 0 != (ret = usb_claim_interface(udev, camera_dev->config[0].interface->altsetting->bInterfaceNumber)) )
    fprintf(stderr,"SetupCameraHandle Error:  usb_claim_interface returns %d (%s)", ret, usb_strerror());
  (*hnd).m_hDevice = udev;
  load_buffer_file("/Users/timo/Dropbox/xenics/xccfgh5_2.ttb");
  // 20110420:  HROE note:  confirmed this ttb file is what X-Control uses in 
  //            X-Control/Settings/auto_8637.xcf
  // (following is XCC_SetControlBits that calls XCC_CmdFPGA, 
  //  deconstructed to be simpler)
  unsigned short value;
  value = ( ((Reg1_Continuous & 0xff) << 8) | (0 ? 1:0) );
  send_command_to_FPGA(CC_CTRL | ( value >> 7) | (value  & 0x01) );
  value = ( ((Reg1_ExternalTrigger & 0xff) << 8) | (0 ? 1:0) );
  send_command_to_FPGA(CC_CTRL | ( value >> 7) | (value  & 0x01) );
  // Set nominal ADC settings, these come from sample.cpp
  xcc_ret |= (XCCERRORs)set_ADC_Vin(ADC_Vin_DEFAULT);
  xcc_ret |= (XCCERRORs)set_ADC_Vref(ADC_Vref_DEFAULT);
  xcc_ret |= (XCCERRORs)set_Vdet_comA(Vdet_comA_DEFAULT);
  xcc_ret |= (XCCERRORs)set_Vdet_comB(Vdet_comB_DEFAULT);
  unsigned int initialIntegrationTimeMillisec = 50;
  _set_integration_time_millisec(initialIntegrationTimeMillisec);
  // TODO: need to do a lot of experimenting with SetCommandWord9808
  set_commandword9808();
  xcc_ret |= (XCCERRORs)set_fan(1);
  xcc_ret |= set_WOI(0, 0, MAXWIDTH-1, MAXHEIGHT-1);
  // hroe:  only included case for our cameras
  // adjust the framerate if the window of interest is changed
  // consider 350 Hz as quickest frame rate
  xcc_ret |= set_frame_rate(350, MAXWIDTH, MAXHEIGHT);
  xcc_ret |= set_image_source_to_camera();
  xcc_ret |= set_capture_mode();
  xcc_ret |= (XCCERRORs)set_pwm(0);
  take_dummy_frame();  
  return;
}

void close_camera()
{
  usb_release_interface((*hnd).m_hDevice, 0);
  usb_close((*hnd).m_hDevice);
  free(hnd);
  return;
}
