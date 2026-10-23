// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

#define C_START 1
#define C_DATA 2
#define C_END 3
#define T_SIZE 0
#define T_NAME 1
#define MAX_DATA (MAX_PAYLOAD_SIZE - 3) // C L2 L1 + dados


static int buildControlPacket(unsigned char C, long size, const char *name, unsigned char *pkt)
{
    int len = 0;
    pkt[len++] = C;

    pkt[len++] = T_SIZE;
    pkt[len++] = 4;
    for (int shift = 24; shift >= 0; shift -= 8)
        pkt[len++] = (size >> shift) & 0xFF;

    int nameLen = strlen(name);
    if (nameLen > 255)
        nameLen = 255;
    pkt[len++] = T_NAME;
    pkt[len++] = nameLen;
    memcpy(pkt + len, name, nameLen);
    len += nameLen;

    return len;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer llParameters = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout,
    };
    strcpy(llParameters.serialPort, serialPort);

    unsigned char packet[MAX_PAYLOAD_SIZE];

    if (strcmp(role, "tx") == 0)
    {
        FILE *f = fopen(filename, "rb");
        if (f == NULL)
        {
            perror(filename);
            return;
        }
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        rewind(f);

        if (llOpenTx(llParameters) < 0)
        {
            fclose(f);
            return;
        }

        int len = buildControlPacket(C_START, fileSize, filename, packet);
        if (llSend(packet, len) < 0)
        {
            printf("Transfer failed (START)\n");
            fclose(f);
            return;
        }

        int k;
        while ((k = fread(packet + 3, 1, MAX_DATA, f)) > 0)
        {
            packet[0] = C_DATA;
            packet[1] = k >> 8;   // L2
            packet[2] = k & 0xFF; // L1
            if (llSend(packet, k + 3) < 0)
            {
                printf("Transfer failed (DATA)\n");
                fclose(f);
                return;
            }
        }
        fclose(f);

        len = buildControlPacket(C_END, fileSize, filename, packet);
        if (llSend(packet, len) < 0)
        {
            printf("Transfer failed (END)\n");
            return;
        }

        printf("File sent (%ld bytes)\n", fileSize);
        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0)
    {
        if (llOpenRx(llParameters) < 0)
            return;

        FILE *f = fopen(filename, "wb");
        if (f == NULL)
        {
            perror(filename);
            return;
        }

        long fileSize = -1, received = 0;
        for (;;)
        {
            int len = llReceive(packet);
            if (len < 0)
            {
                printf("Transfer failed\n");
                fclose(f);
                return;
            }

            if (packet[0] == C_START)
            {
                // Ler os TLV
                for (int i = 1; i + 2 <= len; i += 2 + packet[i + 1])
                {
                    unsigned char T = packet[i], L = packet[i + 1];
                    if (T == T_SIZE)
                    {
                        fileSize = 0;
                        for (int j = 0; j < L; j++)
                            fileSize = (fileSize << 8) | packet[i + 2 + j];
                    }
                    else if (T == T_NAME)
                    {
                        printf("Receiving file %.*s\n", L, (char *)packet + i + 2);
                    }
                }
                printf("File size: %ld bytes\n", fileSize);
            }
            else if (packet[0] == C_DATA)
            {
                int k = 256 * packet[1] + packet[2];
                if (k != len - 3)
                {
                    printf("Data packet size mismatch (K=%d, got %d)\n", k, len - 3);
                    fclose(f);
                    return;
                }
                fwrite(packet + 3, 1, k, f);
                received += k;
            }
            else if (packet[0] == C_END)
            {
                break;
            }
        }
        fclose(f);

        if (received != fileSize)
            printf("WARNING: received %ld bytes, expected %ld\n", received, fileSize);
        else
            printf("File received OK (%ld bytes)\n", received);

        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
