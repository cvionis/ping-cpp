#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <vector>

#pragma comment(lib, "iphlpapi.lib") // link against "iphlapi.lib" for IcmpCreateFile()
#pragma comment(lib, "ws2_32.lib")   // link against "ws2_32.lib"  for InetPtonW

bool isRunning{ true };

BOOL WINAPI HandlerRoutine(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        isRunning = false;
        return TRUE;
    default:
        return FALSE;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " IP address" << std::endl;
        return EXIT_FAILURE;
    }

    // Create a ICMP context from which to issue ICMP echo requests
    HANDLE hIcmp{ IcmpCreateFile() };
    if (hIcmp == INVALID_HANDLE_VALUE)
    {
        std::cout << "Unable to open handle. GetLastError(): " << GetLastError() << std::endl;
        return EXIT_FAILURE;
    }

    // Parse destination IP address
    INT destAddr{ InetPtonA(AF_INET, argv[1], &destAddr) };
    if (destAddr == INADDR_NONE)
    {
        std::cout << "Usage: " << argv[0] << " IP address" << std::endl;
        return EXIT_FAILURE;
    }

    constexpr WORD payloadSize{ 32 };
    unsigned char payload[payloadSize]{ 60 }; 

    // Reply buffer consisting of 1 echo reply, payload data, and 8 additional bytes needed to store potential error msg
    constexpr DWORD replySize{ sizeof(ICMP_ECHO_REPLY) + payloadSize + 8};
    unsigned char replyBuffer[replySize]{};

    int timeOutMs{ 1000 };

    int packetsSent{ 0 };
    int packetsReceived{ 0 };

    std::vector<int>rttList{};

    SetConsoleCtrlHandler(HandlerRoutine, TRUE);

    std::cout << "Pinging " << argv[1] << " with " << payloadSize << (payloadSize == 1 ? " byte " : " bytes ") << "of data:" << std::endl;
    while (isRunning)
    {
        // Make an ICMP echo request
        DWORD replyCount{ IcmpSendEcho(hIcmp, destAddr, payload, payloadSize, NULL, replyBuffer, replySize, timeOutMs) };
        if (replyCount)
        {
			PICMP_ECHO_REPLY pEchoReply{ (PICMP_ECHO_REPLY)replyBuffer };
            std::cout << "Reply from " << argv[1] << ": bytes=" << pEchoReply->DataSize << " time=" << pEchoReply->RoundTripTime << "ms TTL=" << (int)pEchoReply->Options.Ttl << std::endl;

            ++packetsReceived;
            rttList.push_back(pEchoReply->RoundTripTime);
        }
        else
            std::cout << "PING: transmit failed.\n";

        ++packetsSent;
        Sleep(timeOutMs);
    }

    // Print summary statistics for sent ICMP echo requests when program stops running
    int packetsLost{ packetsSent - packetsReceived };
    int percentLost{ (packetsLost ? static_cast<int>(packetsSent / packetsLost) * 100 : 0 )};

    std::cout << "\nPing statistics for " << argv[1] << ':' << std::endl;
    std::cout << "\tPackets: Sent = " << packetsSent << ", Received = " << packetsReceived << ", Lost = " << packetsLost << " (" << percentLost << "% loss)," << std::endl;

    int rttMin{ *std::min_element(rttList.begin(), rttList.end()) };
    int rttMax{ *std::max_element(rttList.begin(), rttList.end()) };
    int rttAvg{ (rttMin + rttMax) / 2 };

    std::cout << "Approximate round trip times in milli-seconds:" << std::endl;
    std::cout << "\tMinimum = " << rttMin << "ms, Maximum = " << rttMax << "ms, Average = " << rttAvg << "ms" << std::endl;

    return 0;
}
