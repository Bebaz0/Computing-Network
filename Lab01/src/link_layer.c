// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm.h"
#include "state_machine.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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

    if (alarmSetup() < 0) {
    closeSerialPort();
    return -1;
}

    for (int tries = 0; tries <= llParameters.nRetransmissions; tries++) {
        if (sendSuperisionFrame(A_TX, C_SET) != 5) {
            closeSerialPort();
            return -1;
        }

        StateMachine sm;
        smInit(&sm, A_TX, C_UA);
        alarmStart(llParameters.timeout);

        while (!alarmFired) {
            unsigned char byte;
            int n = readByteSerialPort(&byte);

            if (n < 0) {
                if (errno == EINTR)
                    continue;  // o alarme interrompeu a leitura

                perror("readByteSerialPort");
                alarmStop();
                closeSerialPort();
                return -1;
            }

            if (n > 0 && smProcessByte(&sm, byte)) {
                alarmStop();
                printf("UA received\n");
                return 0;  // manter a porta aberta
            }
        }

        alarmStop();
        printf("No valid UA (try %d/%d)\n",
            tries + 1, llParameters.nRetransmissions + 1);
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

    StateMachine sm;
    smInit(&sm, A_TX, C_SET);

    for (;;) {
        unsigned char byte;
        int n = readByteSerialPort(&byte);

        if (n < 0) {
            perror("readByteSerialPort");
            closeSerialPort();
            return -1;
        }
        if (n == 0)
            continue;

        if (smProcessByte(&sm, byte)) {
            if (sendSuperisionFrame(A_TX, C_UA) != 5) {
                closeSerialPort();
                return -1;
            }
            return 0;  
        }
    }

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
