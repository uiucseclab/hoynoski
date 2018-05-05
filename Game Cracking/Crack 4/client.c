// https://msdn.microsoft.com/en-us/library/windows/desktop/ms737629(v=vs.85).aspx
// run with flag -lws2_32 (includes winsock api)
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x501
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <winerror.h>
#include <string.h>


#define IP_ADDRESS 			"127.0.0.1" 
#define PORT_NUM			"8080"
#define WINSOCK_VERSION 	MAKEWORD(2,2)
#define RECV_BUFFER_LENGTH 	512
#define TERMINATE_BYTE		0x00 
#define DEFAULT_BYTE		0xFF

void printLastError(const char * functionCall){
	// <-------- BEGIN Retrieve Error Message -------->
	LPTSTR errorMessage = NULL;
	printf("%s returned error code: %i\n", functionCall, WSAGetLastError());
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				  NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 0, NULL);
	fprintf(stderr, "More info: %s\n", errorMessage);
	LocalFree(errorMessage);
	// <--------- END Retrieve Error Message --------->
	//exit(1);
}

void disconnect(SOCKET * sock){
	printf("Closing socket...");
	if(closesocket(*sock) != 0){
		printf("FAILED.\n");
		printLastError("closesocket()");
	}
	printf("DISCONNECTED.\n");
}

void cleanup(){
	printf("Cleaning up Winsock envirnonment...");
	if(WSACleanup() != 0){
		printf("FAILED.\n");
		printLastError("WSACleanup()");
	}
	printf("done.\n");
	exit(1);
}

void initWinsock(WSADATA * wsaData){
	int passResult;
	//Initialize Winsock, everyone must do this to begin
	printf("Initializing Winsock...");
	passResult = WSAStartup(WINSOCK_VERSION, wsaData);	// Requests for version 2.2 of Winsock on the system
	if(passResult != 0){
		printf("FAILED.\n");
		printLastError("WSAStartup()");
	}
	printf("done.\n"); //Initialized Winsock correctly!
}

void createBlankTCPSocket(SOCKET * sock, struct addrinfo addressInfo){
	// <------------------------------------------ BEGIN Creating a Socket ------------------------------------------>
	printf("Creating a blank socket....");
	if((*sock = WSASocketA(addressInfo.ai_family, 
						   addressInfo.ai_socktype, 
						   addressInfo.ai_protocol, 
						   NULL, 0, 0)) == INVALID_SOCKET){
		printf("FAILED.\n");
		printLastError("WSASocketA()");
		cleanup();
	}
	printf("done.\n");
	/* 
	 * 	Note:
	 *		Golf King client uses IPPROTO_IP for the type of protocol to use. Because we are using AF_INET (IPv4) with 
	 *		SOCK_STREAM, it would choose the protocol for us (if we used IPPROTO_IP), which would default to TCP.
	 *		References: http://stackoverflow.com/questions/24590818/what-is-the-difference-between-ipproto-ip-and-ipproto-raw
	 */
	// <------------------------------------------- END Creating a Socket ------------------------------------------->
}

void connectSocket(SOCKET * sock, struct addrinfo addressInfo){
	// <---------------------------------------- BEGIN Connection of Socket ----------------------------------------->

	//printf("Raw: %x, Address: %s\n", server.sin_addr, inet_ntoa(server.sin_addr));
	printf("Connecting to %s through port %s...", IP_ADDRESS, PORT_NUM);
	if(WSAConnect(*sock, addressInfo.ai_addr, (int)addressInfo.ai_addrlen, NULL, NULL, NULL, NULL) != 0){
		printf("FAILED.\n");
		printLastError("WSAConnect()");
		cleanup();
	}
	printf("CONNECTED.\n");
	// <----------------------------------------- END Connection of Socket ------------------------------------------>
}

int sendData(SOCKET * sock, char data[], int dataLength){
	WSABUF DataBuf;
	LPDWORD bytesSent;
	int result;

	DataBuf.len = dataLength;
	DataBuf.buf = &data[0];
	printf("Sending \"%s\" to server...", data);
	if((result = WSASend(*sock, &DataBuf, 1, bytesSent, 0, NULL, NULL)) != 0){
		printf("FAILED.\n");
		printLastError("WSASend()");
		disconnect(sock);
		cleanup();
	}
	printf("done.\n\tSent a total of %i bytes.\n", *bytesSent);
	return result;
}

void recvData(SOCKET * sock, char buf[RECV_BUFFER_LENGTH], LPDWORD bytesRead){
	int result;
	DWORD flags = 0;
	WSABUF dataBuf;

	dataBuf.len = RECV_BUFFER_LENGTH;
	dataBuf.buf = &buf[0];
	printf("Receiving data...");
	if((result = WSARecv(*sock, &dataBuf, 1, bytesRead, &flags, NULL, NULL)) != 0){
		printf("FAILED.\n");
		printLastError("WSARecv()");
		disconnect(sock);
		cleanup();
	}
	printf("RECIEVED!\nPacket Information:\nHex: ");
	for(int i = 0; i < *bytesRead; i++){
		BYTE next = buf[i];
		printf("%02x ", next);
	}
	printf("\n");
}

void generateAddrInfo(struct addrinfo ** result){
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	printf("Getting address information...");
	if((getaddrinfo(IP_ADDRESS, PORT_NUM, &hints, result)) != 0){
		printf("FAILED.\n");
		printLastError("getaddrinfo()");
		cleanup();
	}
	printf("done.\n");
}

void buildAndConnect(WSADATA * wsaData, SOCKET * sock){
	struct addrinfo * addressInfoList = NULL;

	initWinsock(wsaData);
	generateAddrInfo(&addressInfoList);
	createBlankTCPSocket(sock, *addressInfoList);
	// If socket fails, try trying move nodes in the addressInfo list.
	connectSocket(sock, *addressInfoList);

	freeaddrinfo(addressInfoList); // if manually done, dont forget this call.
}

void main(void) {
	WSADATA wsaData;
	SOCKET sock = INVALID_SOCKET;
	
	buildAndConnect(&wsaData, &sock);

	char input[1024] = "HELLO WORLD.\n";
	sendData(&sock, input, (int)strlen(&input[0]));

	char output[RECV_BUFFER_LENGTH];
	LPDWORD read;
	recvData(&sock, output, read);

	disconnect(&sock);
	cleanup();
}