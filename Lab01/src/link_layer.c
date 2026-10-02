// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm.h"

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
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }
    printf("Serial port %s opened\n", llParameters.serialPort);

    if (alarmSetup() < 0)
        return -1;

    for (int tries = 0; tries <= llParameters.nRetransmissions; tries++)
    {
        if (sendSuperisionFrame(A_TX, C_SET) < 0)
            return -1;
        alarmStart(llParameters.timeout);

        // ponytail: fixed 5-byte read, swap for a state machine later
        unsigned char frame[5];
        int received = 0;
        while (!alarmFired && received < 5)
        {
            unsigned char byte;
            if (readByteSerialPort(&byte) > 0) // -1 when the alarm interrupts read()
                frame[received++] = byte;
        }

        if (received == 5 && frame[0] == FLAG && frame[1] == A_TX && frame[2] == C_UA &&
            frame[3] == (A_TX ^ C_UA) && frame[4] == FLAG)
        {
            alarmStop();
            printf("UA received\n");
            return 0; // port stays open for llSend; llCloseTx closes it
        }
        alarmStop();
        printf("No valid UA (try %d/%d)\n", tries + 1, llParameters.nRetransmissions + 1);
    }

    closeSerialPort();
    return -1;
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
        {
            printf("SET frame received\n");
            if (sendSuperisionFrame(A_TX, C_UA) < 0)
                status = -1;
        }
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
