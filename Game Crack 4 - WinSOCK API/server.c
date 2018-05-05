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


#define PORT_NUM			"8080"
#define WINSOCK_VERSION 	MAKEWORD(2,2)
#define RECV_BUFFER_LENGTH 	512 
#define TERMINATE_BYTE		0x00
#define DEFAULT_BYTE		0xFF
#define DEFAULT_BUFLEN		512

void printLastError(const char * functionCall){
	// <-------- BEGIN Retrieve Error Message -------->
	//LPTSTR errorMessage = NULL;
	wchar_t * errorMessage = NULL;
	printf("%s returned error code: %i\n", functionCall, WSAGetLastError());
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				  NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorMessage, 0, NULL);
	fprintf(stderr, "\tMore info: %S", errorMessage);
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

int sendData(SOCKET * sock, char data[DEFAULT_BUFLEN], int dataLength){
	WSABUF DataBuf;
	LPDWORD bytesSent;
	int result;

	DataBuf.len = dataLength;
	DataBuf.buf = data;
	printf("Sending \"");
	for(int i = 0; i < dataLength; i++){
		BYTE next = data[i];
		if(i != dataLength - 1)
			printf("%02x ", next);
		else
			printf("%02x", next);
	}
	printf("\" to server...");
	/*if((result = WSASend(*sock, &DataBuf, 1, bytesSent, 0, NULL, NULL)) != 0){
		printf("FAILED.\n");
		printLastError("WSASend()");
		disconnect(sock);
		cleanup();
	}*/
	if((result = send(*sock, data, dataLength, 0)) == SOCKET_ERROR){
		printf("FAILED.\n");
		printLastError("Send()");
		disconnect(sock);
		cleanup();
	}
	printf("done.\n\tSent a total of %i bytes.\n", dataLength);
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

void generateAddrInfo(struct addrinfo * hints, struct addrinfo ** result){
	/*
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	*/
	ZeroMemory(hints, sizeof(*hints));
	hints->ai_family = AF_INET;
	hints->ai_socktype = SOCK_STREAM;
	hints->ai_protocol = IPPROTO_TCP;
	hints->ai_flags = AI_PASSIVE;

	printf("Getting address information...");
	if((getaddrinfo(NULL, PORT_NUM, hints, result)) != 0){
		printf("FAILED.\n");
		printLastError("getaddrinfo()");
		cleanup();
	}
	printf("done.\n");
}

void bindSocket(SOCKET * listenSock, struct addrinfo * addressInfo){
	printf("Binding listening socket...");
	if(bind(*listenSock, addressInfo->ai_addr, (int)addressInfo->ai_addrlen) == SOCKET_ERROR){
		printf("FAILED.\n");
		printLastError("bind()");
		freeaddrinfo(addressInfo);
		disconnect(listenSock);
		cleanup();
	}
	printf("done.\n");
}

void createListenSock(SOCKET * listenSock){
	printf("Putting listen socket in listening phase...");
	if(listen(*listenSock, SOMAXCONN) == SOCKET_ERROR){
		printf("FAILED.\n");
		printLastError("listen()");
		disconnect(listenSock);
		cleanup();
	}
	printf("done.\n");
}

void acceptNewClient(SOCKET * listenSock, SOCKET * client){
	printf("Waiting for incoming connection...");
	if((*client = WSAAccept(*listenSock, NULL, NULL, NULL, 0)) == INVALID_SOCKET){
		printf("FAILED. Recieved Invalid_Socket :(\n");
		printLastError("WSAAccept()");
		disconnect(listenSock);
		cleanup();
	}
	printf("FOUND.\n");
}

void configureListenSock(WSADATA * wsaData, SOCKET * listenSock){
	struct addrinfo * addressInfoList = NULL;
	struct addrinfo hints;

	initWinsock(wsaData);
	generateAddrInfo(&hints, &addressInfoList);
	createBlankTCPSocket(listenSock, *addressInfoList);
	bindSocket(listenSock, addressInfoList); //unable to bind, check other entries in addressInfo list.
	createListenSock(listenSock);

	freeaddrinfo(addressInfoList); // if manually done, dont forget this call.
	freeaddrinfo(&hints);
}

void main(void) {
	WSADATA wsaData;
	SOCKET listenSock = INVALID_SOCKET, client = INVALID_SOCKET;
	
	configureListenSock(&wsaData, &listenSock);
	acceptNewClient(&listenSock, &client);

	char buf[RECV_BUFFER_LENGTH];
	LPDWORD read;
	recvData(&client, buf, read);

	char sendBuf[DEFAULT_BUFLEN] = {0x00, 0x00, 0x00, 0x0b, 0x6d, 0x65, 0x62, 0x70, 0x3a, 0x2f, 0x2f, 0x01, 0x02, 0x03, 0x04};
	sendData(&client, sendBuf, 15);

	while(1){}
	disconnect(&listenSock);
	cleanup();
}