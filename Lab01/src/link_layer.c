// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm.h"
#include "state_machine.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256
#define FLAG 0x7E
#define A_TX 0x03 // comandos do Tx / respostas do Rx
#define A_RX 0x01 // comandos do Rx / respostas do Tx
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define C_I(n) ((n) << 7)
#define C_RR(n) (0xAA | (n))
#define C_REJ(n) (0x54 | (n))
#define MAX_REJ 10 // maximo de REJs por trama antes de desistir
// Inatividade maxima do Rx: maior que o tempo que o Tx insiste numa trama
// ((nRetransmissions + 1) * timeout), para o Rx nunca desistir antes do Tx
#define RX_IDLE ((params.nRetransmissions + 2) * params.timeout)

static LinkLayer params;
static int seq = 0; // Tx: Ns da proxima trama I; Rx: Ns esperado
static int discReceived = 0; // Rx: DISC chegou durante o llReceive

static int iSent = 0, retransmissions = 0, timeouts = 0, rejReceived = 0 , iReceived = 0, duplicates = 0, rejSent = 0;


static int writeAll(const unsigned char *buf, int n)
{
    int sent = 0;
    while (sent < n)
    {
        int w = writeBytesSerialPort(buf + sent, n - sent);
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += w;
    }
    return sent;
}

static int sendSupervisionFrame(unsigned char A, unsigned char C)
{
    unsigned char frame[5];
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = C;
    frame[3] = A ^ C; // BCC
    frame[4] = FLAG;

    int bytesWritten = writeAll(frame, 5);
    if (bytesWritten < 0)
    {
        perror("writeAll");
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

// Le bytes ate a maquina de estados completar uma trama.
// Devolve 1 se recebeu uma trama, 0 em timeout, -1 em erro.
static int waitFrame(StateMachine *sm, int seconds)
{
    alarmStart(seconds);

    while (!alarmFired)
    {
        unsigned char byte;
        int n = readByteSerialPort(&byte);

        if (n < 0)
        {
            if (errno == EINTR)
                continue; // o alarme interrompeu a leitura

            perror("readByteSerialPort");
            alarmStop();
            return -1;
        }

        if (n > 0 && smProcessByte(sm, byte))
        {
            alarmStop();
            return 1;
        }
    }

    alarmStop();
    return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{
    params = llParameters;
    seq = 0;

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }
    printf("Serial port %s opened\n", llParameters.serialPort);

    if (alarmSetup() < 0)
    {
        closeSerialPort();
        return -1;
    }

    for (int tries = 0; tries <= llParameters.nRetransmissions; tries++)
    {
        if (sendSupervisionFrame(A_TX, C_SET) != 5)
        {
            closeSerialPort();
            return -1;
        }

        StateMachine sm;
        smInit(&sm, A_TX, C_UA);
        alarmStart(llParameters.timeout);

        while (!alarmFired)
        {
            unsigned char byte;
            int n = readByteSerialPort(&byte);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue; // o alarme interrompeu a leitura

                perror("readByteSerialPort");
                alarmStop();
                closeSerialPort();
                return -1;
            }

            if (n > 0 && smProcessByte(&sm, byte))
            {
                alarmStop();
                printf("UA received\n");
                return 0; // manter a porta aberta
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
    params = llParameters;
    seq = 0;
    discReceived = 0;

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    if (alarmSetup() < 0) // usado no llCloseRx
    {
        closeSerialPort();
        return -1;
    }

    StateMachine sm;
    smInit(&sm, A_TX, C_SET);

    for (;;)
    {
        unsigned char byte;
        int n = readByteSerialPort(&byte);

        if (n < 0)
        {
            perror("readByteSerialPort");
            closeSerialPort();
            return -1;
        }
        if (n == 0)
            continue;

        if (smProcessByte(&sm, byte))
        {
            if (sendSupervisionFrame(A_TX, C_UA) != 5)
            {
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
    if (buf == NULL || bufSize <= 0 || bufSize > MAX_PAYLOAD_SIZE)
        return -1;

    // FLAG A C BCC1 | dados + BCC2 (cada um pode virar 2 bytes) | FLAG
    unsigned char frame[2 * (MAX_PAYLOAD_SIZE + 1) + 5];
    int len = 0;
    frame[len++] = FLAG;
    frame[len++] = A_TX;
    frame[len++] = C_I(seq);
    frame[len++] = A_TX ^ C_I(seq); // BCC1

    unsigned char bcc2 = 0;
    for (int i = 0; i < bufSize; i++)
        bcc2 ^= buf[i];

    // Byte stuffing dos dados e do BCC2
    for (int i = 0; i <= bufSize; i++)
    {
        unsigned char byte = (i < bufSize) ? buf[i] : bcc2;
        if (byte == FLAG || byte == ESC)
        {
            frame[len++] = ESC;
            frame[len++] = byte ^ 0x20;
        }
        else
        {
            frame[len++] = byte;
        }
    }
    frame[len++] = FLAG;

    // So os timeouts gastam tentativas: um REJ prova que a ligacao esta viva
    int tries = 0, rejs = 0;
    for (int sends = 0; tries <= params.nRetransmissions; sends++)
    {
        if (sends > 0)
            retransmissions++;

        if (writeAll(frame, len) != len)
        {
            perror("writeAll");
            return -1;
        }
        iSent++;
        printf("Sent I frame Ns=%d (%d bytes)\n", seq, bufSize);

        StateMachine sm;
        smInit(&sm, A_TX, SM_ANY_C);
        alarmStart(params.timeout);

        while (!alarmFired)
        {
            unsigned char byte;
            int n = readByteSerialPort(&byte);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue; // o alarme interrompeu a leitura

                perror("readByteSerialPort");
                alarmStop();
                return -1;
            }
            if (n == 0 || !smProcessByte(&sm, byte))
                continue;

            if (sm.c == C_RR(seq ^ 1))
            {
                alarmStop();
                printf("RR%d received\n", seq ^ 1);
                seq ^= 1;
                return bufSize;
            }
            if (sm.c == C_REJ(seq))
            {
                rejReceived++;
                printf("REJ%d received\n", seq);
                if (++rejs >= MAX_REJ)
                {
                    alarmStop();
                    printf("Too many REJ (%d), giving up\n", MAX_REJ);
                    return -1;
                }
                break; // retransmitir ja
            }

            smInit(&sm, A_TX, SM_ANY_C); // ignorar outras tramas (ex: RR repetido)
        }

        if (alarmFired)
        {
            timeouts++;
            tries++;
            printf("Timeout waiting for RR (try %d/%d)\n",
                   tries, params.nRetransmissions + 1);
        }
        alarmStop();
    }

    return -1;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    StateMachine sm;
    smInit(&sm, A_TX, SM_ANY_C);

    for (;;)
    {
        int r = waitFrame(&sm, RX_IDLE);
        if (r < 0)
            return -1;
        if (r == 0)
        {
            printf("Rx timeout: no frames for %d s\n", RX_IDLE);
            return -1;
        }

        if (sm.c == C_DISC)
        {
            // O Tx desistiu e esta a fechar: o llCloseRx responde
            printf("DISC received during transfer\n");
            discReceived = 1;
            return -1;
        }
        else if (sm.c == C_SET)
        {
            printf("Repeated SET received\n");
            if (sendSupervisionFrame(A_TX, C_UA) != 5)
                return -1;
        }
        else if (sm.c == C_I(0) || sm.c == C_I(1))
        {
            int ns = sm.c >> 7;
            int len = sm.dataLen - 1; // ultimo byte e o BCC2

            unsigned char bcc2 = 0;
            for (int i = 0; i < len; i++)
                bcc2 ^= sm.data[i];

            if (ns != seq)
            {
                // Duplicado (o nosso RR perdeu-se): descartar, mas confirmar
                duplicates++;
                printf("Duplicate I frame Ns=%d\n", ns);
                if (sendSupervisionFrame(A_TX, C_RR(seq)) != 5)
                    return -1;
            }
            else if (len <= 0 || bcc2 != sm.data[len])
            {
                rejSent++;
                printf("BCC2 error in I frame Ns=%d\n", ns);
                if (sendSupervisionFrame(A_TX, C_REJ(seq)) != 5)
                    return -1;
            }
            else
            {
                memcpy(packet, sm.data, len);
                iReceived++;
                seq ^= 1;
                printf("Received I frame Ns=%d (%d bytes)\n", ns, len);
                if (sendSupervisionFrame(A_TX, C_RR(seq)) != 5)
                    return -1;
                return len;
            }
        }

        smInit(&sm, A_TX, SM_ANY_C);
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    printf("Statistics: %d I frames sent, %d retransmissions, %d timeouts, %d REJ received\n",
           iSent, retransmissions, timeouts, rejReceived);

    for (int tries = 0; tries <= params.nRetransmissions; tries++)
    {
        if (sendSupervisionFrame(A_TX, C_DISC) != 5)
        {
            closeSerialPort();
            return -1;
        }

        StateMachine sm;
        smInit(&sm, A_RX, C_DISC);
        alarmStart(params.timeout);

        while (!alarmFired)
        {
            unsigned char byte;
            int n = readByteSerialPort(&byte);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue; // o alarme interrompeu a leitura

                perror("readByteSerialPort");
                alarmStop();
                closeSerialPort();
                return -1;
            }

            if (n > 0 && smProcessByte(&sm, byte))
            {
                alarmStop();
                printf("DISC received\n");
                int ret = sendSupervisionFrame(A_RX, C_UA) == 5 ? 0 : -1;
                closeSerialPort();
                return ret;
            }
        }

        alarmStop();
        printf("No valid DISC (try %d/%d)\n",
               tries + 1, params.nRetransmissions + 1);
    }

    closeSerialPort();
    return -1;
}

int llCloseRx()
{
    printf("Statistics: %d I frames received, %d duplicates, %d REJ sent\n",
           iReceived, duplicates, rejSent);

    // Esperar pelo DISC do Tx (se ainda nao chegou no llReceive)
    StateMachine sm;
    smInit(&sm, A_TX, SM_ANY_C);

    while (!discReceived)
    {
        int r = waitFrame(&sm, RX_IDLE);
        if (r <= 0)
        {
            if (r == 0)
                printf("Rx timeout: no DISC for %d s\n", RX_IDLE);
            closeSerialPort();
            return -1;
        }

        if (sm.c == C_DISC)
            break;

        if (sm.c == C_I(0) || sm.c == C_I(1))
        {
            //confirmar outra vez
            duplicates++;
            if (sendSupervisionFrame(A_TX, C_RR(seq)) != 5)
            {
                closeSerialPort();
                return -1;
            }
        }

        smInit(&sm, A_TX, SM_ANY_C);
    }
    printf("DISC received\n");

    for (int tries = 0; tries <= params.nRetransmissions; tries++)
    {
        if (sendSupervisionFrame(A_RX, C_DISC) != 5)
        {
            closeSerialPort();
            return -1;
        }

        smInit(&sm, A_RX, C_UA);
        alarmStart(params.timeout);

        while (!alarmFired)
        {
            unsigned char byte;
            int n = readByteSerialPort(&byte);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue; // o alarme interrompeu a leitura

                perror("readByteSerialPort");
                alarmStop();
                closeSerialPort();
                return -1;
            }

            if (n > 0 && smProcessByte(&sm, byte))
            {
                alarmStop();
                printf("UA received\n");
                closeSerialPort();
                return 0;
            }
        }

        alarmStop();
        printf("No valid UA (try %d/%d)\n",
               tries + 1, params.nRetransmissions + 1);
    }

    closeSerialPort();
    return -1;
}
