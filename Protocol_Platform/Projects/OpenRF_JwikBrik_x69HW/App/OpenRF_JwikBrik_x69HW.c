/*************************************************************************************
**																					**
**	Sample Application for RL78 and SX1231   										**
** 																					**
**************************************************************************************
**																					**
** Written By:	Steve Montgomery													**
**				Digital Six Laboratories LLC										**
** (c)2013 Digital Six Labs, All rights reserved									**
**																					**
**************************************************************************************/
//
// Revision History
//
// Revision		Date	Revisor		Description
// ===================================================================================
// ===================================================================================
																							
/*! \mainpage OpenRF™ Open Source Embedded Wireless Protocol Platform
 *
 * \section intro_sec Introduction
 *
 * OpenRF™ is an open source embedded wireless networking protocol platform.   Using the MIT
 * License, OpenRF™ is not only free to use, but there are no commercial restrictions on its use.
 * The protocol platform is layered as follows:
 *
 * Platform layers:
 *	<ul>
 *	<li>OpenRF MAC Layer
 *		<ol>
 * 		<li>Packet types: Unicast with acknowledgement, Unicast without acknowledgement, Multicast
 * 		<li>32-bit unique MAC addresses
 * 		<li>Simple to use API
 * 		</ol>
 * 	<li>Radio API
 * 		<ol>
 * 		<li>Unified API that can support any embedded wireless RFIC
 * 		<li>SX1231 currently supported
 * 		<li>Supports FHSS and single channel operation
 * 		</ol>
 * 	<li>Microcontroller API
 * 		<ol>
 * 		<li>Unified API that can support any microcontroller
 * 		<li>RL78 G13 currently supported
 * 		</ol>
 *	</ul>
 *
 * \section cod_doc Code documentation
 *
 * \ref OpenRF Documentation for the OpenRF MAC API
 *
 * \ref Radio Documentation for the SX1231 based radio API
 *
 * \ref Microcontroller Documentation for the RL78 based microcontroller API
 *
 * \section info Important Information
 *
 * \version 0.A
 *
 * \date 10/26/2013
 *
 * \copyright (c) 2013 Digital Six Laboratories LLC
 *
 * \authors Steve Montgomery
 *
 *
 */
#include "..\..\..\SourceCode\MicrocontrollerAPI\RL78\iodefine.h"
#include "..\..\..\SourceCode\OpenRF_MAC\openrf_mac.h"
#include "..\..\..\SourceCode\MicrocontrollerAPI\RL78\microapi.h"
#include "..\..\..\SourceCode\Utility\atprocessor.h"
typedef enum
{
	kDisabled,
	kPlus1,
	kPlus2,
	kPlus3,
	kEnabled
} tAtStates;
#define ACK 0x06
#define NACK 0x15



#define kMajorSoftwareVersion 0
#define kMinorSoftwareVersion 1

// *****************************************
// IO Pin Definitions

#define pinLockLED P1_bit.no4
#define pinNetworkMode P1_bit.no3
#define pinDI0 P12_bit.no0
#define pinDI1 P2_bit.no6
#define pinDI2 P3_bit.no0
#define pinDI3 P5_bit.no1
#define pinDI4 P14_bit.no7

#define pinDO0 P6_bit.no2
#define pinDO1 P3_bit.no1
#define pinDO2 P12_bit.no2
#define pinDO3 P1_bit.no7
#define pinDO4 P1_bit.no6


// *****************************************
// AT Commands

#define kATCommandCount 24
U8* atCommands[kATCommandCount] = {"SL","NA","DL","CN","RE","EK","BD","NB","SB","SS","TE","%V","VR","WS","RR","SP","TL","TT","GS","TP","TS","AR","AT","HT"};
// AT Commands
enum
{
	kGetMACAddressCommand,
	kGetSetNetworkAddressCommand,
	kGetSetDestinationMACAddressCommand,
	kExitCommandMode,
	kResetToFactoryCommand,
	kGetSetEncryptionKeyCommand,
	kGetSetDataRateCommand,
	kGetSetParityCommand,
	kGetSetStopBitsCommand,
	kGetRSSICommand,
	kGetTemperatureCommand,
	kGetPowerSupplyCommand,
	kGetFirmwareVersion,
	kWriteSettings,
	kGetSetRadioRate,
	kGetSetPacketType,
	kGetSetTriggerLevel,
	kGetSetTriggerTimeout,
	kGetSenderMAC,
	kGetSetTransmitPower,
	kSetTimeReference,
	kGetSetAckRetriesCommand,
	kGetSetAckTimeoutCommand,
	kGetSetHopTable,
	kNullCommand = 0xff
};

// IO Commands
enum
{
	kReadAnalog,
	kReadDigital,
	kSetDigital,
	kSetDigitalTriggerCmd,
	kSetAnalogTriggerCmd
};

#ifdef CPPAPP
//Initialize global constructors
extern "C" void __main()
{
  static int initialized;
  if (! initialized)
    {
      typedef void (*pfunc) ();
      extern pfunc __ctors[];
      extern pfunc __ctors_end[];
      pfunc *p;

      initialized = 1;
      for (p = __ctors_end; p > __ctors; )
	(*--p) ();

    }
}
#endif 

UU32 _macAddress;
UU32 _networkId;
UU32 _destinationAddress;
UU128 _encryptionKey;
U16 _transmitTimer;
U8 _operatingMode;
U8 _transmitTriggerLevel;
U16 _transmitTriggerTimeout;
U16 _transmitTriggerTimer;
U8 _transmitTriggerTimerActive =0;
U8 _packetType=0;
U8 _packetReceived = 0;
U8 _receivePacketDataBuffer[63];
U8 _receivePacketCount;
U8 _receivePacketType;
UU32 _receivePacketSenderMAC;
U8 _digitalTriggers[5];
UU16 _analogTriggers[6];
U8 _radioDataRate;
U8 _transmitPower;
U8 _uartBaudRate;
U8 _quiet;
U8 _ackRetries;
U16 _ackTimeout;
U8 _hopTable;
extern UU32 _RTCDateTimeInSecs;
// 0 = KRF-TC2
// 1 = KRF-TCMP2
#define kRadioType 1

void WriteCharToUart(U8 ch)
{
	if(ch<0x0a)
		WriteCharUART1(ch+'0');
	else
		WriteCharUART1(ch-10+'A');
}
void WriteU32ToUart(UU32 val)
{
	int i;
	for(i=0;i<4;i++)
	{
		WriteCharToUart(val.U8[3-i]>>4);
		WriteCharToUart(val.U8[3-i]&0x0f);
	}
}
U32 parseHexU32( U8 *str )
{
    U32 value = 0;


    while(*str!=0x00)
    {
		switch( *str )
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				value = value << 4 | ((*str-'0') & 0xf);
				break;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				value = value << 4 | (10 + (*str-'A') & 0xf);
				break;
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				value = value << 4 | (10 + (*str-'a') & 0xf);
				break;
			default:
				return value;
		}
		str++;
    }
    return value;
}
U8 ReadU32FromUart(U32 *val)
{
	U8 str[9];
	U8 i;

	for(i=0;(i<8 && IsATBufferNotEmpty());i++)
	{
		if(IsATBufferNotEmpty())
			str[i] = GetATBufferCharacter();
	}
	str[i]=0;
	*val= parseHexU32(&str[0]);
	return i;
}
U16 parseHexU16( U8 *str )
{
    U16 value = 0;


    while(*str!=0x00)
    {
		switch( *str )
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				value = value << 4 | ((*str-'0') & 0xf);
				break;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				value = value << 4 | (10 + (*str-'A') & 0xf);
				break;
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				value = value << 4 | (10 + (*str-'a') & 0xf);
				break;
			default:
				return value;
		}
		str++;
    }
    return value;
}
U8 ReadU16FromUart(U16 *val)
{
	U8 str[5];
	U8 i;

	for(i=0;(i<4 && IsATBufferNotEmpty());i++)
	{
		if(IsATBufferNotEmpty())
			str[i] = GetATBufferCharacter();
	}
	str[i]=0;
	*val= parseHexU16(&str[0]);
	return i;
}
U8 parseHexU8( U8 *str )
{
    U8 value = 0;

    while(*str!=0x00)
    {
		switch( *str )
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				value = value << 4 | ((*str-'0') & 0xf);
				break;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				value = value << 4 | (10 + (*str-'A') & 0xf);
				break;
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				value = value << 4 | (10 + (*str-'a') & 0xf);
				break;
			default:
				return value;
		}
		str++;
    }
    return value;
}
U8 ReadU8FromUart(U8 *val)
{
	U8 str[3];
	U8 i,bo;

	for(i=0;(i<2 && IsATBufferNotEmpty());i++)
	{
		bo=IsATBufferNotEmpty();
		if(bo)
			str[i] = GetATBufferCharacter();
	}
	str[i]=0;
	*val= parseHexU8(&str[0]);
	return i;
}
// Callback handler for AT command management
void ATCommand(U8 commandNumber)
{
	U8 retVal,bo;
	UU32 val128[4];
	U32 val32;
	tOpenRFInitializer ini;
	switch(commandNumber)
	{
	case kGetMACAddressCommand:
		WriteU32ToUart(_macAddress);
		break;
	case kGetSetNetworkAddressCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteU32ToUart(_networkId);
		}
		else
		{
			retVal = ReadU32FromUart(&_networkId.U32);
			if(retVal)
			{
				ini.AckRetries = _ackRetries;
				ini.AckTimeout = _ackTimeout;
				ini.ChannelCount = 25;
				ini.DataRate = _radioDataRate;
				ini.EncryptionKey = _encryptionKey;
				ini.GfskModifier = 1;
				ini.HopTable = _hopTable;
				ini.MacAddress = _macAddress;
				ini.NetworkId = _networkId;
				ini.StartChannel = 0;
				OpenRFInitialize(ini);
			}
		}
		break;
	case kGetSetDestinationMACAddressCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteU32ToUart(_destinationAddress);
		}
		else
		{
			retVal = ReadU32FromUart(&val32);
			if(retVal)
				_destinationAddress.U32 = val32;
		}
		break;
	case kExitCommandMode:
		ExitCommandMode();
		break;
	case kResetToFactoryCommand:
		break;
	case kGetSetRadioRate:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharUART1(_radioDataRate+'0');
		}
		else
		{
				retVal = ReadU8FromUart(&_radioDataRate);
				if(retVal)
				{
					ini.AckRetries = _ackRetries;
					ini.AckTimeout = _ackTimeout;
					ini.ChannelCount = 25;
					ini.DataRate = _radioDataRate;
					ini.EncryptionKey = _encryptionKey;
					ini.GfskModifier = 1;
					ini.HopTable = _hopTable;
					ini.MacAddress = _macAddress;
					ini.NetworkId = _networkId;
					ini.StartChannel = 0;
					OpenRFInitialize(ini);
				}
		}
		break;
	case kGetSetEncryptionKeyCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteU32ToUart(_encryptionKey.UU32[0]);
			WriteU32ToUart(_encryptionKey.UU32[1]);
			WriteU32ToUart(_encryptionKey.UU32[2]);
			WriteU32ToUart(_encryptionKey.UU32[3]);
		}
		else
		{

			retVal = ReadU32FromUart(&val32);
			if(retVal)
				_encryptionKey.UU32[0].U32 = val32;
			retVal &= ReadU32FromUart(&val32);
			if(retVal)
				_encryptionKey.UU32[1].U32 = val32;
			retVal &= ReadU32FromUart(&val32);
			if(retVal)
				_encryptionKey.UU32[2].U32 = val32;
			retVal &= ReadU32FromUart(&val32);
			if(retVal)
				_encryptionKey.UU32[3].U32 = val32;
			if(retVal)
			{
				ini.AckRetries = _ackRetries;
				ini.AckTimeout = _ackTimeout;
				ini.ChannelCount = 25;
				ini.DataRate = _radioDataRate;
				ini.EncryptionKey = _encryptionKey;
				ini.GfskModifier = 1;
				ini.HopTable = _hopTable;
				ini.MacAddress = _macAddress;
				ini.NetworkId = _networkId;
				ini.StartChannel = 0;
				OpenRFInitialize(ini);
			}
		}
		break;
	case kGetSetDataRateCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharUART1(GetUART1BaudRate()+'0');
		}
		else
		{
			if(ReadU8FromUart(&_uartBaudRate))
			SetUART1BaudRate(_uartBaudRate);
		}
		break;
	case kGetSetParityCommand:
		break;
	case kGetSetStopBitsCommand:
		break;
	case kGetRSSICommand:
		WriteCharToUart(RadioReadRSSIValue()>>4);
		WriteCharToUart(RadioReadRSSIValue()&0xff);
		break;
	case kGetTemperatureCommand:
		WriteCharToUart(RadioGetTemperature()>>4);
		WriteCharToUart(RadioGetTemperature()&0xff);
		break;
	case kGetPowerSupplyCommand:
		break;
	case kGetFirmwareVersion:
		WriteCharUART1(kMajorSoftwareVersion+'0');
		WriteCharUART1(kMinorSoftwareVersion+'0');
		break;
	case kWriteSettings:
		ErasePersistentArea();
		WritePersistentValue(0,_networkId.U8,4);
		WritePersistentValue(4,&_destinationAddress.U8[0],4);
		WritePersistentValue(8,&_encryptionKey.U8[0],16);
		WritePersistentValue(24,&_operatingMode,1);
		bo=GetUART1BaudRate();
		WritePersistentValue(25,&bo,1);
		WritePersistentValue(26,&_transmitTriggerLevel,1);
		WritePersistentValue(27,(U8*)(&_transmitTriggerTimeout),1);
		// mark the persistent memory as initialized so it will be read on the next reset
		bo=0x55;
		WritePersistentValue(254,&bo,1);
		break;
	case kGetSetPacketType:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharUART1(_packetType+'0');
		}
		else
		{
			ReadU8FromUart(&_packetType);
		}
		break;
	case kGetSetTriggerLevel:
		bo = IsATBufferNotEmpty();
		if(!bo)
				{
					WriteCharToUart(_transmitTriggerLevel>>4);
					WriteCharToUart(_transmitTriggerLevel&0x0f);
				}
				else
				{
					ReadU8FromUart(&_transmitTriggerLevel);
				}
		break;
	case kGetSetTriggerTimeout:
		bo = IsATBufferNotEmpty();
		if(!bo)
				{
					WriteCharToUart(_transmitTriggerTimeout>>4);
					WriteCharToUart(_transmitTriggerTimeout&0x0f);
				}
				else
				{
					ReadU16FromUart(&_transmitTriggerTimeout);
				}
		break;
	case kGetSenderMAC:
		WriteU32ToUart(_receivePacketSenderMAC);
		break;
	case kGetSetTransmitPower:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharToUart(_transmitPower>>4);
			WriteCharToUart(_transmitPower&0x0f);
		}
		else
		{
			if(ReadU8FromUart(&_transmitPower))
				RadioSetTxPower(_transmitPower);
		}
		break;
	case kSetTimeReference:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			if(ReadU32FromUart(&val32))
			{
				_RTCDateTimeInSecs.U32 = val32;
			}
		}
		break;
	case kGetSetAckRetriesCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharToUart(_ackRetries);
		}
		else
		{
			if(ReadU8FromUart(&_ackRetries))
			{
				ini.AckRetries = _ackRetries;
				ini.AckTimeout = _ackTimeout;
				ini.ChannelCount = 25;
				ini.DataRate = _radioDataRate;
				ini.EncryptionKey = _encryptionKey;
				ini.GfskModifier = 1;
				ini.HopTable = _hopTable;
				ini.MacAddress = _macAddress;
				ini.NetworkId = _networkId;
				ini.StartChannel = 0;
				OpenRFInitialize(ini);
			}

		}
		break;
	case kGetSetAckTimeoutCommand:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharToUart(((UU16)_ackTimeout).U8[1]);
			WriteCharToUart(((UU16)_ackTimeout).U8[0]);
		}
		else
		{
			if(ReadU16FromUart(&_ackTimeout))
			{
				ini.AckRetries = _ackRetries;
				ini.AckTimeout = _ackTimeout;
				ini.ChannelCount = 25;
				ini.DataRate = _radioDataRate;
				ini.EncryptionKey = _encryptionKey;
				ini.GfskModifier = 1;
				ini.HopTable = _hopTable;
				ini.MacAddress = _macAddress;
				ini.NetworkId = _networkId;
				ini.StartChannel = 0;
				OpenRFInitialize(ini);
			}

		}
		break;
	case kGetSetHopTable:
		bo = IsATBufferNotEmpty();
		if(!bo)
		{
			WriteCharToUart(_hopTable);
		}
		else
		{
			if(ReadU8FromUart(&_hopTable))
			{
				ini.AckRetries = _ackRetries;
				ini.AckTimeout = _ackTimeout;
				ini.ChannelCount = 25;
				ini.DataRate = _radioDataRate;
				ini.EncryptionKey = _encryptionKey;
				ini.GfskModifier = 1;
				ini.HopTable = _hopTable;
				ini.MacAddress = _macAddress;
				ini.NetworkId = _networkId;
				ini.StartChannel = 0;
				OpenRFInitialize(ini);
			}

		}
		break;
	case kNullCommand:
		WriteCharUART1('O');
		WriteCharUART1('K');
		break;
	}
}
// Load in presets from NV memory
void LoadPresets()
{
	if(ReadPersistentValue(254)==INITIALIZEDVALUE)
	{
		_networkId.U8[3] = ReadPersistentValue(0);
		_networkId.U8[2] = ReadPersistentValue(1);
		_networkId.U8[1] = ReadPersistentValue(2);
		_networkId.U8[0] = ReadPersistentValue(3);
		_destinationAddress.U8[3] = ReadPersistentValue(4);
		_destinationAddress.U8[2] = ReadPersistentValue(5);
		_destinationAddress.U8[1] = ReadPersistentValue(6);
		_destinationAddress.U8[0] = ReadPersistentValue(7);
		_encryptionKey.U8[15] = ReadPersistentValue(8);
		_encryptionKey.U8[14] = ReadPersistentValue(9);
		_encryptionKey.U8[13] = ReadPersistentValue(10);
		_encryptionKey.U8[12] = ReadPersistentValue(11);
		_encryptionKey.U8[11] = ReadPersistentValue(12);
		_encryptionKey.U8[10] = ReadPersistentValue(13);
		_encryptionKey.U8[9] = ReadPersistentValue(14);
		_encryptionKey.U8[8] = ReadPersistentValue(15);
		_encryptionKey.U8[7] = ReadPersistentValue(16);
		_encryptionKey.U8[6] = ReadPersistentValue(17);
		_encryptionKey.U8[5] = ReadPersistentValue(18);
		_encryptionKey.U8[4] = ReadPersistentValue(19);
		_encryptionKey.U8[3] = ReadPersistentValue(20);
		_encryptionKey.U8[2] = ReadPersistentValue(21);
		_encryptionKey.U8[1] = ReadPersistentValue(22);
		_encryptionKey.U8[0] = ReadPersistentValue(23);
		_quiet = ReadPersistentValue(24);
		SetUART1BaudRate(ReadPersistentValue(25));
		_transmitTriggerLevel = ReadPersistentValue(26);
		_transmitTriggerTimeout = ReadPersistentValue(27);
		_ackRetries = ReadPersistentValue(28);
		((UU16)_ackTimeout).U8[1] = ReadPersistentValue(29);
		((UU16)_ackTimeout).U8[0] = ReadPersistentValue(30);
		_hopTable = ReadPersistentValue(31);
	}
	else
	{

		_macAddress.U32 = 0x11223344;
		_networkId.U32 = 0xaa555aa5;
		_destinationAddress.U32=0x44332211;
		_networkId.U32 = 0x11332244;
		_encryptionKey.UU32[3].U32 = 0x1C1D1E1F;
		_encryptionKey.UU32[2].U32 = 0x1E1F1A1B;
		_encryptionKey.UU32[1].U32 = 0x1A1B1C1D;
		_encryptionKey.UU32[0].U32 = 0x11223344;
		_quiet=0;

		_transmitTriggerLevel=4;
		_transmitTriggerTimeout = 200;
		_packetType = kMulticastPacketType;
		_radioDataRate = k38400BPS;
	}

}
// Print a string to the UART
void PrintString(U8 *str)
{
	if(_quiet) return;
	while(*str!='\0')
		WriteCharUART1(*str++);
}
// Print a string to the UART with CRLF
void PrintLine(U8 *str)
{
	if(_quiet) return;
	PrintString(str);
	WriteCharUART1('\n');
	WriteCharUART1('\r');
}
// Send a packet over the radio using UART1 received data
void SendPacketFromUART1Data(void)
{
	U8 i;
	U8 count;
	// maximum size of packet is 63 bytes
	U8 buff[63];

	count = BufferCountUART1();
	// never send more than the trigger level number of bytes
	if(count>_transmitTriggerLevel)
		count = _transmitTriggerLevel;
	for(i=0;i<count;i++)
		buff[i] = ReadCharUART1();
	// wait until we are in a state to send the packet.  We must call OpenRFLoop() continuously to update the internal state machine to ensure that
	// ReadyToSend doesn't get stuck on a fail.
	while(!OpenRFReadyToSend())
		OpenRFLoop();
	// TODO: Set the preamable count
	OpenRFSendPacket(_destinationAddress,_packetType,count,buff,128);
}


/*****************************************************************************************************************************
 ** 		MAIN FUNCION																									**
 *****************************************************************************************************************************/
int main(void)
{

	U16 ab;
	UU16 analogSample;
	U8 byteCount,i;
	U8 respBuffer[16];
	U8 digitalSample;
	U8 temp;
	tOpenRFInitializer ini;

	// InitializeMicroApi() is called in hardware_setup.c as part of the start-up code.

	ErasePersistentArea();
	LoadPresets();

	ini.NetworkId = _networkId;
	ini.MacAddress = _macAddress;
	ini.EncryptionKey = _encryptionKey;
	ini.DataRate = k38400;
	OpenRFInitialize(ini);
	_transmitTimer=0;
	ATInitialize(atCommands, kATCommandCount,ATCommand);
	EnableInterrupts;
	PrintLine("rfBrick version 0.1A");
    while (1)
    {
    	OpenRFLoop();

    	ATProcess();
    	// if the network mode pin is high, we are an IO slave, otherwise we are a transparent UART radio
    	// The master is a transparent UART radio.
    	if(pinNetworkMode)
    	{
    		// Here, we are operating as an IO slave.  We will process requests from our master and sense/change our IO accordingly
    		if(_packetReceived)
    		{
    			_packetReceived = 0;
    			// the first byte is the command
    			switch(_receivePacketDataBuffer[0])
    			{
    			case kReadAnalog:
    				byteCount=1;
					respBuffer[0] = NACK;
    				if( (_receivePacketDataBuffer[1]>4) && (_receivePacketCount>=2) )
    				{
						AnalogSetInputChannel(_receivePacketDataBuffer[1]);
						analogSample.U16 = AnalogGet10BitResult();
						respBuffer[0] = ACK;
						respBuffer[1] = analogSample.U8[1];
						respBuffer[2] = analogSample.U8[0];
						byteCount = 3;
    				}
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,byteCount,&respBuffer[0]);
    				break;
    			case kReadDigital:

    				respBuffer[0] = ACK;
    				if( _receivePacketCount>=2)
    				{
    					byteCount = 2;
						switch(_receivePacketDataBuffer[1])
						{
						case 0:
							digitalSample = pinDI0;
							break;
						case 1:
							digitalSample = pinDI1;
							break;
						case 2:
							digitalSample = pinDI2;
							break;
						case 3:
							digitalSample = pinDI3;
							break;
						case 4:
							digitalSample = pinDI4;
							break;
						default:
							byteCount = 1;
							respBuffer[0] = NACK;
							break;
						}
    				}
    				else
    				{
    					byteCount = 1;
    					respBuffer[0] = NACK;
    				}
    				respBuffer[1] = digitalSample;
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,byteCount,&respBuffer[0]);
    				break;
    			case kSetDigital:
    				respBuffer[0] = ACK;
    				if( _receivePacketCount>=2)
    				{
    					byteCount = 2;
						switch(_receivePacketDataBuffer[1])
						{
						case 0:
							if(_receivePacketDataBuffer[2]>0)
								pinDO0 = 1;
							else
								pinDO0 = 0;
							break;
						case 1:
							if(_receivePacketDataBuffer[2]>0)
								pinDO1 = 1;
							else
								pinDO1 = 0;
							break;
						case 2:
							if(_receivePacketDataBuffer[2]>0)
								pinDO2 = 1;
							else
								pinDO2 = 0;
							break;
						case 3:
							if(_receivePacketDataBuffer[2]>0)
								pinDO3 = 1;
							else
								pinDO3 = 0;
							break;
						case 4:
							if(_receivePacketDataBuffer[2]>0)
								pinDO4 = 1;
							else
								pinDO4 = 0;
							break;
						default:
							byteCount = 1;
							respBuffer[0] = NACK;
						}
    				}
    				else
    				{
    					byteCount = 1;
    					respBuffer[0] = NACK;
    				}
    				respBuffer[1] = digitalSample;
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,byteCount,&respBuffer[0]);
    				break;
    			case kSetDigitalTriggerCmd:
    				byteCount=1;
					if( (_receivePacketDataBuffer[1]<5) && (_receivePacketCount>=2) )
					{
						respBuffer[1] = NACK;
						_digitalTriggers[_receivePacketDataBuffer[1]] = _receivePacketDataBuffer[2];
						byteCount = 2;
					}
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,byteCount,&respBuffer[0]);
    				break;
    			case kSetAnalogTriggerCmd:
    				byteCount=1;
					respBuffer[0] = NACK;
					if( (_receivePacketDataBuffer[1]<6) && (_receivePacketCount>=2) )
					{
						respBuffer[0] = ACK;
						byteCount = 2;
						_analogTriggers[_receivePacketDataBuffer[1]].U8[1] = _receivePacketDataBuffer[2];
						_analogTriggers[_receivePacketDataBuffer[1]].U8[2] = _receivePacketDataBuffer[3];
					}
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,byteCount,&respBuffer[0]);
    				break;
    			default:
					respBuffer[0] = NACK;
    				OpenRFSendPacket(_receivePacketSenderMAC,_packetType,1,&respBuffer[0]);
    				break;
    			}
    		}
    	}
    	else
    	{
    		if(_packetReceived)
    		{
    			// send whatever we have in the receive buffer to the UART
    			_packetReceived = 0;
    			for(i=0;i<_receivePacketCount;i++)
    				WriteCharUART1(_receivePacketDataBuffer[i]);
    		}
    		// Here, if we are not in AT command mode, we need to take whatever data we receive from the UART and forward it.  The
    		// data will be forwarded to the module selected by the destination ID.
    		if(ATGetState()!=kEnabled)
    		{
    			byteCount = BufferCountUART1();
    			if(byteCount>_transmitTriggerLevel)
    			{
    				SendPacketFromUART1Data();
    			}
    			else
    			{
    				// look at the buffer and see what we should do
    				if(byteCount>0)
    				{
    					if(!_transmitTriggerTimerActive)
    					{
    						_transmitTriggerTimerActive = 1;
    						_transmitTriggerTimer = 0;
    					}
    				}
    				if(_transmitTriggerTimerActive)
    					if(_transmitTriggerTimer>_transmitTriggerTimeout)
    					{
    						SendPacketFromUART1Data();

    						// de-activate the timer
    						_transmitTriggerTimerActive = 0;
    					}
    			}
    		}
    	}

    }

    return 0;
}

/*****************************************************************************************************************************
 ** 		EVENT HANDLERS																									**
 *****************************************************************************************************************************/
extern void NotifyMacPacketReceived(tPacketTypes packetType,UU32 sourceMACAddress,U8 length, U8 xdata *SDU, U8 rssi)
{
	int i;
	for(i=0;i<length;i++)
		_receivePacketDataBuffer[i] = *(SDU++);
	_receivePacketSenderMAC.U32 = sourceMACAddress.U32;
	_receivePacketCount = length;
	_receivePacketType = packetType;
	_packetReceived = 1;
}
extern void NotifyMacReceiveError()
{

}
extern void NotifyMac1Second()
{
}
extern void NotifyMacPacketSent()
{

}
extern void NotifyMacPacketSendError(tTransmitErrors error)
{

}
extern void NotifyMac1MilliSecond()
{
	_transmitTimer++;
	_transmitTriggerTimer++;
}
