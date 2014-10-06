///////////////////////////////////////////////////////////////////////////////
//
// name: ADAQDigitizer.cc
// date: 02 Oct 14
// auth: Zach Hartwig
//
// desc: 
//
///////////////////////////////////////////////////////////////////////////////

// C++
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>

// CAEN
extern "C" {
#include "CAENDigitizer.h"
}

// ADAQ
#include "ADAQDigitizer.hh"


ADAQDigitizer::ADAQDigitizer(ZBoardType Type, int ID, uint32_t Address)
  : ADAQVBoard(Type, ID, Address),
    NumChannels(0), NumADCBits(0), MinADCBit(0), MaxADCBit(0)
{;}
// Initialize the pointers used during the data acquisition process
// to readout the digitized waveforms from the V1720 to the PC
//    Buffer_Py(NULL), EventPointer_Py(NULL), EventWaveform_Py(NULL)


ADAQDigitizer::~ADAQDigitizer()
{;}


int ADAQDigitizer::OpenLink()
{
  CommandStatus = -42;
  
  if(LinkEstablished){
    if(Verbose)
      std::cout << "ADAQDigitizer : Error opening link! Link is already open!"
		<< std::endl;
  }
  else{
    CommandStatus = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, 
					    0, 
					    0, 
					    BoardAddress,
					    &BoardHandle);
  }
  
  if(CommandStatus == CAEN_DGTZ_Success){

    LinkEstablished = true;

    if(Verbose){

      CAEN_DGTZ_BoardInfo_t BoardInformation;
      CAEN_DGTZ_GetInfo(BoardHandle, &BoardInformation);

      std::cout << "ADAQDigitizer : Link successfully established!\n"
		<< "--> Board     : " << BoardInformation.ModelName << "\n"
		<< "--> Channels  : " << BoardInformation.Channels << "\n"
		<< "--> AMC FW    : " << BoardInformation.ROC_FirmwareRel << "\n"
		<< "--> ROC FW    : " << BoardInformation.AMC_FirmwareRel << "\n"
		<< "--> ADC bits  : " << BoardInformation.ADC_NBits << "\n"
		<< "--> Serial #  : " << BoardInformation.SerialNumber << "\n"
		<< "\n"
		<< "--> Board address : 0x" << std::setw(8) << std::setfill('0') << BoardAddress << "\n"
		<< "--> Board ID      : " << BoardID << "\n"
		<< "--> Board handle  : " << BoardHandle << "\n"
		<< std::endl;
      
      NumChannels = BoardInformation.Channels;
      NumADCBits = BoardInformation.ADC_NBits;
      MinADCBit = 0;
      MaxADCBit = NumADCBits - 1;
    }
  }
  else
    if(Verbose and !LinkEstablished)
      std::cout << "ADAQDigitizer : Error opening link! Returned error code: " << CommandStatus << "\n"
		<< std::endl;
  
  return CommandStatus;
}


int ADAQDigitizer::CloseLink()
{
  CommandStatus = -42;
  
  if(LinkEstablished)
    CommandStatus = CAEN_DGTZ_CloseDigitizer(BoardHandle);
  else
    if(Verbose)
      std::cout << "ADAQDigitizer : Error closing link! Link is already closed!\n"
		<< std::endl;
  
  if(CommandStatus == CAEN_DGTZ_Success){

    LinkEstablished = false;

    if(Verbose)
      std::cout << "ADAQDigitizer : Link successfully closed!\n"
		<< std::endl;
  }
  else
    if(Verbose and LinkEstablished)
      std::cout << "ADAQDigitizer : Error closing link! Error code: " << CommandStatus << "\n"
		<< std::endl;
  
  return CommandStatus;
}


int ADAQDigitizer::Initialize()
{
  // Reset the board firmware
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_SW_RESET_ADD, 0x00000000);
  
  // Set the VME control: all disabled, enable BERR
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_VME_CONTROL_ADD, 0x00000010);
  
  // Set front panel I/O controls 
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_FRONT_PANEL_IO_CTRL_ADD, 0x00000000);
  
  // Set the trigger source enable mask 
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD, 0xC0000080);
  
  // Set the channel trigger enable mask
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_FP_TRIGGER_OUT_ENABLE_ADD, 0x00000000);
  
  // Set the channel configuration
  CAEN_DGTZ_WriteRegister(BoardHandle, CAEN_DGTZ_BROAD_CH_CTRL_ADD, 0x00000050);
}


int ADAQDigitizer::SetRegisterValue(uint32_t Addr32, uint32_t Data32)
{ 
  CommandStatus = CAEN_DGTZ_WriteRegister(BoardHandle, Addr32, Data32); 
  return CommandStatus;
}


int ADAQDigitizer::GetRegisterValue(uint32_t Addr32, uint32_t *Data32)
{
  CommandStatus = CAEN_DGTZ_ReadRegister(BoardHandle, Addr32, Data32);
  return CommandStatus;
}


bool ADAQDigitizer::CheckRegisterForWriting(uint32_t Addr32)
{ return true; }


////////////////////////
// Triggering methods //
////////////////////////

int ADAQDigitizer::EnableAutoTrigger(uint32_t ChannelEnableMask)
{ return SetChannelSelfTrigger(CAEN_DGTZ_TRGMODE_ACQ_ONLY, ChannelEnableMask); }


int ADAQDigitizer::DisableAutoTrigger(uint32_t ChannelEnableMask)
{ return SetChannelSelfTrigger(CAEN_DGTZ_TRGMODE_DISABLED, ChannelEnableMask); }


int ADAQDigitizer::EnableExternalTrigger(std::string SignalLogic)
{
  CommandStatus = SetExtTriggerInputMode(CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT);
  
  // Get the value of the front panel I/O control register
  uint32_t FrontPanelIOControlRegister = CAEN_DGTZ_FRONT_PANEL_IO_CTRL_ADD;
  uint32_t FrontPanelIOControlValue = 0;
  CommandStatus = GetRegisterValue(FrontPanelIOControlRegister, &FrontPanelIOControlValue);
  
  // When Bit[0] of return value == 0, NIM logic is used for input; so
  // clear Bit[0] using bitwise ops if "NIM" is specified
  if(SignalLogic=="NIM")
    FrontPanelIOControlValue &= ~(1<<0);
  else if(SignalLogic=="TTL")
    FrontPanelIOControlValue |= 1<<0;
  else
    if(Verbose)
      std::cout << "ADAQDigitizer : Error! Unsupported external trigger logic ("
		<< SignalLogic << ") was specified!" << "\n"
		<< "                Select 'NIM' or 'TTL'!\n"
		<< std::endl;
  
  CommandStatus = SetRegisterValue(FrontPanelIOControlRegister, FrontPanelIOControlValue);
  return CommandStatus;
}


int ADAQDigitizer::DisableExternalTrigger()
{ 
  CommandStatus = SetExtTriggerInputMode(CAEN_DGTZ_TRGMODE_DISABLED); 
  return CommandStatus;
}


int ADAQDigitizer::EnableSWTrigger()
{ 
  CommandStatus = SetSWTriggerMode(CAEN_DGTZ_TRGMODE_ACQ_ONLY);
  return CommandStatus;
}


int ADAQDigitizer::DisableSWTrigger()
{ 
  CommandStatus = SetSWTriggerMode(CAEN_DGTZ_TRGMODE_DISABLED);
  return CommandStatus;
}


int ADAQDigitizer::SetTriggerEdge(int Channel, string TriggerEdge)
{
  CommandStatus = -42;

  if(TriggerEdge == "Rising")
    CommandStatus = SetTriggerPolarity(Channel, 
				       CAEN_DGTZ_TriggerOnRisingEdge);
  else if(TriggerEdge == "Falling")
    CommandStatus = SetTriggerPolarity(Channel, 
				       CAEN_DGTZ_TriggerOnFallingEdge);
  else
    if(Verbose)
      std::cout << "ADAQDigitizer : Error! Unsupported trigger edge type ("
		<< TriggerEdge << ") was specified!\n"
		<< "               Select 'Rising' or 'Falling'\n"
		<< std::endl;
  
  return CommandStatus;
}


int ADAQDigitizer::SetTriggerCoincidence(bool Enable, int Level)
{
  CommandStatus = -42;

  if(Enable){
    
    uint32_t TriggerSourceEnableMask = 0;

    uint32_t TriggerCoincidenceLevel_BitShifted = Level << 24;

    CommandStatus = GetRegisterValue(0x810C,&TriggerSourceEnableMask);
    
    TriggerSourceEnableMask = TriggerSourceEnableMask | TriggerCoincidenceLevel_BitShifted;
    
    CommandStatus = SetRegisterValue(0x810C,TriggerSourceEnableMask);
  }
  return CommandStatus;
}


/////////////////////////
// Acquisition methods //
/////////////////////////

int ADAQDigitizer::SetAcquisitionMode(string AcqMode)
{ 
  CommandStatus = -42;

  if(AcqMode == "Software")
    CommandStatus = SetAcquisitionMode(CAEN_DGTZ_SW_CONTROLLED);
  else if(AcqMode == "SIn")
    CommandStatus = SetAcquisitionMode(CAEN_DGTZ_S_IN_CONTROLLED);
  else
    if(Verbose)
      std::cout << "ADAQDigitizer : Error! Unsupported acquisition mode ("
		<< AcqMode << ") was specified!\n"
		<< "                Select 'Software' or 'SIn'.\n"
		<< std::endl;

  return CommandStatus;
}


int ADAQDigitizer::SetZSMode(string ZSMode)
{
  CommandStatus = -42;
  
  if(ZSMode == "None")
    CommandStatus = SetZeroSuppressionMode(CAEN_DGTZ_ZS_NO);
  else if(ZSMode == "ZLE")
    CommandStatus = SetZeroSuppressionMode(CAEN_DGTZ_ZS_ZLE);
  else
    if(Verbose)
      std::cout << "ADAQDigitizer : Error! Unsupported zero suppression mode ("
		<< ZSMode << ") was specified!\n"
		<< "                Select 'None' or 'ZLE'.\n"
		<< std::endl;
  
  return CommandStatus;
}


int ADAQDigitizer::SetZLEChannelSettings(uint32_t Channel, uint32_t Threshold,
					 uint32_t NBackward, uint32_t NForward,
					 bool PosLogic)
{;}


/////////////////////
// Readout methods //
/////////////////////

int ADAQDigitizer::CheckBufferStatus(bool *BufferStatus)
{
  // bit[0] : 0 = memory not full; 1 = memory full
  // bit[1] : 0 = memory not empty; 1 = memory empty
  // bit[2] : 0 = DAC not busy; 1 = DAC busy
  // bit[3] : Reserved
  // bit[4] : Reserved
  // bit[5] : Buffer free error

  CommandStatus = -42;

  // Channel register addresses and channel-to-channel increment
  uint32_t Start = CAEN_DGTZ_CHANNEL_STATUS_BASE_ADDRESS;
  uint32_t Offset = 0x0100;

  for(int ch=0; ch<NumChannels; ch++){

    uint32_t Addr32 = Start + Offset*ch;
    uint32_t Data32 = 0;
    int Status = 0;
    // Skip channels that are not currently enabled
    CommandStatus = GetChannelEnableMask(&Data32);
    if(!(Data32 & (1 << ch)))
      continue;
    
    // Check to see if the 0-th of each channel's status register bit
    // is set; if any of the channel buffers are full then set the
    // BufferFull flag to true
    CommandStatus = CAEN_DGTZ_ReadRegister(BoardHandle, Addr32, &Data32);
    (Data32 & (1 << 0)) ? BufferStatus[ch] = true : BufferStatus[ch] = false;
  }
  return CommandStatus;
}
  

int ADAQDigitizer::GetNumFPGAEvents(uint32_t *Data32)
{ 
  CommandStatus = GetRegisterValue(CAEN_DGTZ_EVENT_STORED_ADD,
				   Data32);
  return CommandStatus;
}

