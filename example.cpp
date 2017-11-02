/*
* © Abaco Systems (2017)
*
* The Software being provided shall be supplied and used in accordance with the attached 
* Software License Agreement. Please note however that the Software being provided is provided 
* WITHOUT WARRANTY LIABILITY INDEMNITY OR SUPPORT OF ANY KIND WHATSOEVER.
*
* Author: ross.newman@abaco.com
* Notes:
*   For testing of the COM interface the Hercules tool is very usefull and will allow 
*   scripted and manual formation of messages (in HEX) to test your IMPI interface.
*
*   https://www.hw-group.com/products/hercules/index_en.html
*
*/

/* Expected output when run on SBC328 in SLOT_1 :
		Abaco Systems IPMI test on slot 0x82 on COM3

		Opening serial port successful
		IPMI Messgae :
		0xa0 0x82 0x18 0x66 0x01 0x02 0x01 0xfc 0xa5
		Awaiting response CTRL+C to finish...
		0xA0 0x1 0x1E 0xE1 0x82 0x0 0x1 0x0 0x0 0x81 0x5 0x3 0x2 0x1D 0x28 0x8 0x0 0x86 0x0 0x0 0x0 0x0 0x0 0x1F 0xA5 0xA6
*/

#include <windows.h>
#include <stdio.h>  

enum {
	SLOT_1 = 0x82,
	SLOT_2 = 0x84,
	SLOT_3 = 0x86,
	SLOT_4 = 0x88,
	SLOT_5 = 0x8A,
	SLOT_6 = 0x8C,
	SLOT_7 = 0x8E,
	SLOT_8 = 0x90
};

enum {
	SERIAL_FRAMING_START     = 0xA0,
	SERIAL_FRAMING_STOP      = 0xA5,
	SERIAL_FRAMING_HANDSHAKE = 0xA6,
	SERIAL_FRAMING_ESCAPE    = 0xAA,
	SERIAL_FRAMING_ASCII_ESC = 0x1B
};

#define COM_PORT "COM3"
#define SLOT SLOT_1

unsigned char calculate_checksum(unsigned char arr[], int len) {
	int sum = 0;
	for (int i = 0; i < (len); ++i) {
		sum += arr[i];
	}
	/* modulo 256 sum */
	sum %= 256;

	char ch = sum;

	/* twos complement */
	unsigned char twoscompl = ~ch + 1;

	return twoscompl;
}

bool valid_checksum(unsigned char arr[], int len) {
	unsigned char ch = 0;

	ch = calculate_checksum(arr, len-1);

	printf("Calculated new checksum : %d \n", ch);

	return arr[len - 1] == ch;
}

int main()
{
	HANDLE hComm;
	bool Status;

	/* our raw IPMI command...
  		Example taken from BMM/BMCSoftware Reference Manual
		BMM / BMC with VITA46.11 Support
		Edition 3 - Section 2.2, sending 'Get Device ID'
	*/
	unsigned char ipmi_message[] = { SLOT, 0x18, 0x00, 0x01, 0x02, 0x01, 0x00 };
	/*                                           ^^^^ Checksum           ^^^^ Checksum */
	int arrsize = sizeof(ipmi_message) / sizeof(ipmi_message[0]);

	/* Running */
	printf("Abaco Systems IPMI test on slot 0x%x on %s\n\n", SLOT, COM_PORT);

	/* open the COM port ready to write */
	hComm = CreateFile("\\\\.\\" COM_PORT, /* port name */
		GENERIC_READ | GENERIC_WRITE,      /* Read/Write */
		0,                                 /* No Sharing */
		NULL,                              /* No Security */
		OPEN_EXISTING,                     /* Open existing port only */
		0,                                 /* Non Overlapped I/O */
		NULL);                             /* Null for Comm Devices */

	if (hComm == INVALID_HANDLE_VALUE) {
		printf("Error in opening serial port\n");
		return -1;
	}
	else
		printf("Opening serial port successful\n");

	/* Calculate first checksum */
	ipmi_message[2] = calculate_checksum(ipmi_message, 2);
	/* Calculate second checksum */
	ipmi_message[arrsize -1] = calculate_checksum(ipmi_message, arrsize - 1);

	printf("IPMI Messgae :\n0xa0 ");
	for (int i = 0; i < (arrsize); ++i) {
		printf("0x%02x ", ipmi_message[i]);
	}
	printf("0xa5\n");

	DWORD dNoOFBytestoWrite;         /* No of bytes to write into the port */
	DWORD dNoOfBytesWritten = 0;     /* No of bytes written to the port */

	DCB dcbSerialParams = { 0 };     /* Initializing DCB structure */
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	Status = GetCommState(hComm, &dcbSerialParams);

	dcbSerialParams.BaudRate = CBR_115200;  /* Setting BaudRate = 115200 */
	dcbSerialParams.ByteSize = 8;           /* Setting ByteSize = 8 */
	dcbSerialParams.StopBits = ONESTOPBIT;  /* Setting StopBits = 1 */
	dcbSerialParams.Parity = NOPARITY;      /* Setting Parity = None */

	SetCommState(hComm, &dcbSerialParams);

	/* Framing START */
	char lpBuffer[1];
	lpBuffer[0] = { (char)SERIAL_FRAMING_START };
	dNoOFBytestoWrite = sizeof(lpBuffer);
	Status = WriteFile(hComm,        /* Handle to the Serial port */
		lpBuffer,     /* Data to be written to the port */
		dNoOFBytestoWrite,  /* No of bytes to write */
		&dNoOfBytesWritten, /* Bytes written */
		NULL);

	/* Framing message */
	dNoOFBytestoWrite = sizeof(ipmi_message);
	Status = WriteFile(hComm,        /* Handle to the Serial port */
		ipmi_message,     /* Data to be written to the port */
		dNoOFBytestoWrite,  /* No of bytes to write */
		&dNoOfBytesWritten, /* Bytes written */
		NULL);

	/* Framing STOP */
	lpBuffer[0] = { (char)SERIAL_FRAMING_STOP };
	dNoOFBytestoWrite = sizeof(lpBuffer);
	Status = WriteFile(hComm,        /* Handle to the Serial port */
		lpBuffer,     /* Data to be written to the port */
		dNoOFBytestoWrite,  /* No of bytes to write */
		&dNoOfBytesWritten, /* Bytes written */
		NULL);

	
	/* Wait for response chars*/
	printf("Awaiting response CTRL+C to finish...\n");

	char TempChar;           /* Temporary character used for reading */
	char SerialBuffer[256];  /* Buffer for storing Rxed Data */
	DWORD NoBytesRead;
	int i = 0;

	do
	{
		ReadFile(hComm,            /* Handle of the Serial port */
			&TempChar,             /* Temporary character */
			sizeof(TempChar),      /* Size of TempChar */
			&NoBytesRead,          /* Number of bytes read */
			NULL);
		printf("0x%hhX ", TempChar);
		SerialBuffer[i] = TempChar; /* Store Tempchar into buffer */
		i++;
	}

	while (NoBytesRead > 0);
	
	CloseHandle(hComm); /* Closing the Serial Port */

	return 0;
} 