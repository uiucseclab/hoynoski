/*
 * Tells the linker to add the "crypt32.lib" library to the list of library dependencies, 
 * as if you had added it in the project properties.
 */
#pragma comment(lib, "advapi32")

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <Wincrypt.h>

#define FORTY_BIT_KEY		0x280000		// 40-bit RC4 session key is generated.
#define DERIVE_KEY_FLAGS	0
#define BIG_BUFFER_SIZE		1024		
#define PUBLIC_KEY 			{0xEA, 0xCA, 0x31, 0x01} // Obtained from reversing in OllyDBG
#define PUBLIC_KEY_LENGTH 	4
#define ENCRYPT_BLOCK_SIZE  8
#define NULL_TERMINATING	0x00

// References: https://msdn.microsoft.com/en-us/library/aa382375(v=vs.85).aspx
void MyHandleError(LPTSTR psz){
    _ftprintf(stderr, TEXT("An error occurred in the program. \n"));
    _ftprintf(stderr, TEXT("%s\n"), psz);
    _ftprintf(stderr, TEXT("Error number %x.\n"), GetLastError());
    _ftprintf(stderr, TEXT("Program terminating. \n"));
    exit(1);
}

void readCSPName(HCRYPTPROV handle, CHAR * buffer, DWORD bufferLength){
	if(CryptGetProvParam(
		handle,
		PP_NAME,
		(BYTE*)buffer,
		&bufferLength,
		0)){
		_tprintf(TEXT("CryptGetProvParam Succeeded.\n"));
		printf("\tProvider name: %s\n", buffer);
	}else{
		MyHandleError(TEXT("Error reading Crypographic Service Provider's (CSP's) name.\n"));
	}
}

void readKeyContainerName(HCRYPTPROV handle, CHAR * buffer, DWORD bufferLength){
	if(CryptGetProvParam(
		handle,
		PP_CONTAINER,
		(BYTE*)buffer,
		&bufferLength,
		0)){
		_tprintf(TEXT("CryptGetProvParam Succeeded.\n"));
		printf("\tKey container name: %s\n", buffer);
	}else{
		MyHandleError(TEXT("Error reading key container name.\n"));
	}
}

void releaseContext(HCRYPTPROV * handle){
	if(CryptReleaseContext(*handle, 0)){
		_tprintf(TEXT("CryptReleaseContext Succeeded.\n"));
	}else{
		MyHandleError(TEXT("Error during CryptReleaseContext!\n"));
	}
}

void destroyContextKeyset(LPCTSTR containerName){
	HCRYPTPROV temp; 	// undefined after call, just for placeholder.
	if(CryptAcquireContext(
		&temp,
		containerName,
		MS_DEF_PROV,
		PROV_RSA_FULL,
		CRYPT_DELETEKEYSET)){
		//_tprintf(TEXT("Deleted the \"%s\" key container.\n")); Works some how?
		printf("\tDeleted the \"%s\" key container.\n", containerName);
		_tprintf(TEXT("CryptAcquireContext Succeeded (Ref: CRYPT_DELETEKEYSET).\n"));
	}else{
		MyHandleError(TEXT("Error during CryptAcquireContext (Ref: CRYPT_DELETEKEYSET)"));
	}
}

void acquireContext(HCRYPTPROV * handle, LPCTSTR containerName){
	if(CryptAcquireContext(
		handle, 
		containerName, 
		MS_DEF_PROV, 
		PROV_RSA_FULL, 
		0)){
		_tprintf(TEXT("CryptAcquireContext Succeeded.\n"));
	}else{
		// No default container was found. Now Attempt to create it.
		_tprintf(TEXT("No default container was found. Attempting to create new one.\n"));
		if(GetLastError() == NTE_BAD_KEYSET){
			if(CryptAcquireContext(
				handle, 
				containerName, 
				MS_DEF_PROV, 
				PROV_RSA_FULL, 
				CRYPT_NEWKEYSET)){
				_tprintf(TEXT("CryptAcquireContext Succeeded.\n"));
			}else{
				MyHandleError(TEXT("Could not create the default key container.\n"));
			}
		}else{
			MyHandleError(TEXT("A general error running CryptAcquireContext.\n"));
			//NTE_EXISTS?
		}
	}
}

void deriveKey(HCRYPTPROV handle, HCRYPTKEY * key, BYTE * password, DWORD passwordLength){
	HCRYPTHASH	hHash;

	// Create a hash environment that deals in MD5.
	if(CryptCreateHash(
		handle,
		CALG_MD5,
		0,
		0,
		&hHash)){
		_tprintf(TEXT("CryptCreateHash Succeeded!.\n\r\tAn empty hash object has been created.\n"));
	}else{
		MyHandleError("Error during CryptCreateHash!\n");
	}

	// Hash the password with MD5.
	if(CryptHashData(
		hHash,
		password,
		passwordLength,
		0)){
		_tprintf(TEXT("CryptHashData Succeeded!\n\r\tThe password has been hashed.\n"));
	}else{
		MyHandleError(TEXT("Error during CryptHashData!\n"));
	}

	if(CryptDeriveKey(
		handle,
		CALG_RC4,
		hHash,
		FORTY_BIT_KEY | DERIVE_KEY_FLAGS,
		key)){
		_tprintf(TEXT("CryptDeriveKey Succeeded!\n\r\tThe key has been derived and is now live!\n"));
	}else{
		MyHandleError(TEXT("Error during CryptDeriveKey!\n"));
	}

	if(hHash){
		if(CryptDestroyHash(hHash)){
			_tprintf(TEXT("CryptDestoryHash Succeeded!\n\r\tHash has been destroyed.\n"));
		}else{
			MyHandleError(TEXT("Error during CryptDestoryHash.\n"));
		}
	}
}

void destroyKey(HCRYPTKEY * key){
	if(*key){
		if(CryptDestroyKey(*key)){
			_tprintf(TEXT("CryptDestroyKey Succeeded!\n\r\tKey has been destroyed.\n"));
		}else{
			MyHandleError(TEXT("Error during CryptDestroyKey.\n"));
		}
	}
}

void openDestination(HANDLE * destination, LPCTSTR destinationName){
	(*destination) = CreateFile(
		destinationName,
		FILE_WRITE_DATA,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if(INVALID_HANDLE_VALUE != (*destination)){
		_tprintf(TEXT("The destination file, %s, is open.\n"), destinationName);
	}else{
		MyHandleError(TEXT("Error opening new destination file!\n"));
	}
}

void openSource(HANDLE * source, LPCTSTR sourceName){
	(*source) = CreateFile(
		sourceName,
		FILE_READ_DATA,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if(INVALID_HANDLE_VALUE != (*source)){
		_tprintf(TEXT("The source encrypted file, %s, is open.\n"), sourceName);
	}else{
		MyHandleError(TEXT("Error opening source chipher text file!\n"));
	}
}

void closeFile(HANDLE source){
	if(!CloseHandle(source))
		MyHandleError(TEXT("Error: Could not close file.\n"));
	_tprintf(TEXT("File Closing Succeeded!\n"));
}

//void decrypt(HCRYPTKEY * key, HANDLE * source, HANDLE * destination){
//	PBYTE pbBuffer 		= NULL;		// typedef BYTE * PBYTE
	/*
	 * Determine the number of bytes to decrypt at a time.
	 * This must be a multiple of ENCRYPT_BLOCK_SIZE.
	 */
/*	DWORD dwBlockLen 	= 1000 - 1000 % ENCRYPT_BLOCK_SIZE;
	DWORD dwBufferLen 	= dwBlockLen;
	DWORD dwCount;

	// Reverse memory to hold decrypte text between destinations.
	if(!(pbBuffer = (PBYTE)malloc(dwBufferLen))){
		MyHandleError(TEXT("Bleh~! Out of Memory!\n"));
	}

	bool fEOF = false;
	do{
		if(!ReadFile(
			*source,
			pbBuffer,
			dwBlockLen,
			&dwCount,
			NULL)){
			MyHandleError(TEXT("Error reading from source file!\n"));
		}

		if(dwCount <= dwBlockLen)
			fEOF = true;

		if(!CryptDecrypt(
			*key,
			0,
			fEOF,
			0,
			pbBuffer,
			&dwCount)){
			MyHandleError(TEXT("Error during CryptDecrypt!\n"));
		}

	}while(fEOF);
}*/

void writeDataToFile(HANDLE destination, PBYTE data, DWORD size){
	DWORD bytesActuallyWrote;
	if(!WriteFile(
		destination,
		data,
		size,
		&bytesActuallyWrote,
		NULL)){
		MyHandleError(TEXT("Error writing data to file.\n"));
	}else{
		if(bytesActuallyWrote != size){
			_tprintf(TEXT("Error: Did not write complete size of data. Wrote %i out of %i"), bytesActuallyWrote, size);
			return;
		}
		_tprintf(TEXT("Wrote all bytes possible to destination file."));
	}
}

void readDataFromFile(HANDLE source, PBYTE * data, DWORD * size){
	if(source == 0){
		_tprintf(TEXT("Error: Unable to read from final as no handle was given."));
		return;
	}

	DWORD * dwHigh = NULL;
	DWORD dwLow = GetFileSize(source, dwHigh);

	if(dwHigh != NULL){
		// TODO: allow bigger files to be decypted.
		_tprintf(TEXT("Error: File too large for single decrypt."));
		return;
	}

	PBYTE buffer = (PBYTE)malloc(dwLow + 1);
	DWORD bytesRead = 0;

	if(!ReadFile(
		source,
		buffer,
		dwLow,
		&bytesRead,
		NULL)){
		MyHandleError(TEXT("Error reading from source file!\n"));
	}else{
		if(bytesRead != dwLow)
			_tprintf(TEXT("Some bytes were NOT read. Read %i out of %i"), bytesRead, dwLow);
		*(buffer + dwLow) = NULL_TERMINATING;
	}

	*data = buffer;
	*size = dwLow;
}

void encrypt(HCRYPTPROV handle, HCRYPTKEY * key, BYTE * plaintext, DWORD * plaintextLength){
	HCRYPTHASH	hHash;

	// Create a hash environment that deals in MD5.
	if(CryptCreateHash(
		handle,
		CALG_MD5,
		0,
		0,
		&hHash)){
		_tprintf(TEXT("CryptCreateHash Succeeded!\n\r\tAn empty hash object has been created.\n"));
	}else{
		MyHandleError("Error during CryptCreateHash!\n");
	}

	if(CryptEncrypt(
		*key,
		hHash,
		TRUE,
		0,
		plaintext,
		plaintextLength,
		*plaintextLength)){
		_tprintf(TEXT("CryptEncrypt Succeeded.\n\r\tData has been encrypted!\n"));
	}else{
		MyHandleError("Error during CryptEncrypt!\n");
	}

	if(hHash){
		if(CryptDestroyHash(hHash)){
			_tprintf(TEXT("CryptDestoryHash Succeeded!\n\r\tHash has been destroyed.\n"));
		}else{
			MyHandleError(TEXT("Error during CryptDestoryHash.\n"));
		}
	}
}

void decrypt(HCRYPTPROV handle, HCRYPTKEY * key, BYTE * encryptedData, DWORD * encryptedDataLength){
	HCRYPTHASH	hHash;

	// Create a hash environment that deals in MD5.
	if(CryptCreateHash(
		handle,
		CALG_MD5,
		0,
		0,
		&hHash)){
		_tprintf(TEXT("CryptCreateHash Succeeded!\n\r\tAn empty hash object has been created.\n"));
	}else{
		MyHandleError("Error during CryptCreateHash!\n");
	}

	if(CryptDecrypt(
		*key,
		hHash,
		TRUE,
		0,
		encryptedData,
		encryptedDataLength)){
		_tprintf(TEXT("CryptDecrypt Succeeded!\n\r\t--- BEGIN Data Read ---\n\r%s\t---- END Data Read ----\n"), encryptedData);
	}

	if(hHash){
		if(CryptDestroyHash(hHash)){
			_tprintf(TEXT("CryptDestoryHash Succeeded!\n\r\tHash has been destroyed.\n"));
		}else{
			MyHandleError(TEXT("Error during CryptDestoryHash.\n"));
		}
	}
}

void main(void){
	// <----------------------------------------- BEGIN Declare and Initialize Varaibles Section ----------------------------------------->
	HCRYPTPROV  handle; 																// Handle on Crypographic Service Provider (CSP) context
	LPCTSTR 	containerName 					= (const char*)"YOUAREGOLFKING\0"; 		// LPCTSTR = const char*, Key container name. This identifies the key container to the CSP.
	CHAR 		name[BIG_BUFFER_SIZE];													// Buffer for parameter catching with read functions (e.g. readCSPName)	
	DWORD		nameLength						= BIG_BUFFER_SIZE;						// Buffer size
	HCRYPTKEY 	key;
	BYTE 		password[PUBLIC_KEY_LENGTH] 	= PUBLIC_KEY;
	HANDLE 		sourceFile						= INVALID_HANDLE_VALUE;
	HANDLE 		destinationFile					= INVALID_HANDLE_VALUE;
	// <------------------------------------------ END Declare and Initialize Varaibles Section ------------------------------------------>

	acquireContext(&handle, containerName);
	
	readCSPName(handle, &name[0], nameLength);
	readKeyContainerName(handle, &name[0], nameLength);

	deriveKey(handle, &key, &password[0], PUBLIC_KEY_LENGTH);

	PBYTE chiperText 		= NULL;
	DWORD chiperTextLength	= 0;
	//LPCTSTR sourceAddress 	= (const char*)"C:/Hangame/KOREAN/Golf/res/Option/Hq.dec";
	//LPCTSTR destAddress		= (const char *)"C:/Hangame/KOREAN/Golf/res/Option/Hq.enc";
	LPCTSTR sourceAddress 	= (const char*)"C:/ijji/ENGLISH/Golf/res/Option/PlayEnv.dec";
	LPCTSTR destAddress		= (const char *)"C:/ijji/ENGLISH/Golf/res/Option/PlayEnv.enc";
	openSource(&sourceFile, sourceAddress);
	openDestination(&destinationFile, destAddress);
	readDataFromFile(sourceFile, &chiperText, &chiperTextLength);

	//decrypt(handle, &key, chiperText, &chiperTextLength);
	encrypt(handle, &key, chiperText, &chiperTextLength);

	writeDataToFile(destinationFile, chiperText, chiperTextLength);

	free(chiperText);

	closeFile(sourceFile);
	closeFile(destinationFile);

	// <--------------------------------------------------------- BEGIN Clean Up --------------------------------------------------------->
	destroyKey(&key);
	releaseContext(&handle);
	destroyContextKeyset(containerName);
	// <---------------------------------------------------------- END Clean Up ---------------------------------------------------------->
}