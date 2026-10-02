// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256
#define FLAG 0x7E
#define A_TX 0x03
#define C_SET 0x03
#define C_UA 0x07


int sendSuperisionFrame(unsigned char A, unsigned char C)
{
    unsigned char frame[5];
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = C;
    frame[3] = A ^ C; // BCC
    frame[4] = FLAG;

    int bytesWritten = writeBytesSerialPort(frame, 5);
    if (bytesWritten < 0)
    {
        perror("writeBytesSerialPort");
        return -1;
    }

    printf("Sent supervision frame: ");
    for (int i = 0; i < 5; i++)
    {
        printf("%02X ", frame[i]);
    }
    printf("\n");

    return bytesWritten;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and send a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    if (sendSuperisionFrame(A_TX, C_SET) < 0)
    {
        perror("sendSuperisionFrame");
        return -1;
    }


    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    unsigned char frame[5];
    size_t received = 0;
    int status = 0;

    while (received < sizeof(frame))
    {
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if (bytes < 0)
        {
            perror("readByteSerialPort");
            status = -1;
            break;
        }

        if (bytes == 0)
            continue;

        frame[received++] = byte;
        printf("Byte received: 0x%02X\n", byte);
    }

    if (status == 0)
    {
        if (frame[0] == FLAG && frame[1] == A_TX && frame[2] == C_SET &&
            frame[3] == (A_TX ^ C_SET) && frame[4] == FLAG)
            printf("SET frame received\n");
        else
        {
            fprintf(stderr, "Invalid SET frame received\n");
            status = -1;
        }
    }

    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);
    return status;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    // TODO: Implement this function

    return 0;
}

int llCloseRx()
{
    // TODO: Implement this function

    return 0;
}
