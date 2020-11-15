#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h> //for I/O
#include <stdlib.h> //for rand/strand
#include <windows.h> //for Win32 API
#include <time.h>    //for using the time as the seed to strand!
#define MAX_SLEEP_TIME 3000 //Miliseconds (1/2 second)
#define MIN_SLEEP_TIME 5
#define MAX_STRING 50
#define True 1
#define False 0
#define N 50
int numvessel;
HANDLE printmutex, haifamutex, *vessels,syncprintmutex;
HANDLE sem[N];
int initGlobalData(int);
void cleanupGlobalData();
DWORD WINAPI Vessel(PVOID), read, written;
HANDLE StdinRead, StdinWrite;      /* pipe for writing parent to child */
HANDLE StdoutRead, StdoutWrite;    /* pipe for writing child to parent */
BOOL success;
TCHAR ProcessName[256];
char message[MAX_STRING];
int calcSleepTime();
void sail_to_eilat(int);
struct tm *local;
void print();

int main(int argc, char *argv[])
{
	int i, vesselID[N],count = 0,id =0;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD ThreadId; //dummy - for Thread Create invokations
	time_t now;
	time(&now);
	local = localtime(&now);
	if (argc != 2)
	{
		fprintf(stderr,"Usage: %s N", argv[0]);
		exit(1);
	}
	if (atoi(argv[1]) < 2 || atoi(argv[1]) > 50)
	{
		fprintf(stderr,"The need to be between 2-50\n");
		exit(1);
	}
	printf("[%02d:%02d:%02d] Haifa Port: The number of vessels is %d\n", local->tm_hour, local->tm_min, local->tm_sec, atoi(argv[1]));
	/* set up security attributes so that pipe handles are inherited */
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };

	/* allocate memory */
	ZeroMemory(&pi, sizeof(pi));
	/* create the pipe for writing from parent to child */
	if (!CreatePipe(&StdinRead, &StdinWrite, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}

	/* create the pipe for writing from child to parent */
	if (!CreatePipe(&StdoutRead, &StdoutWrite, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}
	syncprintmutex = CreateMutex(NULL, False, TEXT("syncprintmutex"));
	if (syncprintmutex == NULL)
	{
		fprintf(stderr, "Error create syncprintmutex\n");
	}
	/* establish the START_INFO structure for the child process */
	GetStartupInfo(&si);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	/* redirect the standard input to the read end of the pipe */
	si.hStdOutput = StdoutWrite;
	si.hStdInput = StdinRead;
	si.dwFlags = STARTF_USESTDHANDLES;

	wcscpy(ProcessName, L".\\Eilat.exe");
	/* create the child process */
	if (!CreateProcess(NULL,
		ProcessName,
		NULL,
		NULL,
		TRUE, /* inherit handles */
		0,
		NULL,
		NULL,
		&si,
		&pi))
	{
		fprintf(stderr, "Process Creation Failed\n");
		return -1;
	}
	/* close the unused ends of the pipe */
	CloseHandle(StdoutWrite);
	CloseHandle(StdinRead);
	printf("[%02d:%02d:%02d] Haifa Port: Send the number of vessels to Eilat for approval... \n", local->tm_hour, local->tm_min, local->tm_sec);
	/* the parent now wants to write to the pipe */
	if (!WriteFile(StdinWrite, argv[1], MAX_STRING, &written, NULL))
		fprintf(stderr, "Error writing to pipe\n");
	/* now read from the pipe */
	success = ReadFile(StdoutRead, message, MAX_STRING, &read, NULL);
	if (success) {
		if (initGlobalData(atoi(argv[1])) == False) {
			fprintf(stderr, "main::Unexpected Error in Global Semaphore Creation\n");
			return 1;
		}
		printf("[%02d:%02d:%02d] Haifa Port: Eilat has given an approval and we can start send the vessels... \n", local->tm_hour, local->tm_min, local->tm_sec);
		numvessel = atoi(argv[1]);
		vessels = calloc(numvessel,sizeof(HANDLE));
		for (i = 0; i < numvessel; i++)
		{
			vesselID[i] = i+1;
			vessels[i] = CreateThread(NULL, 0, Vessel, &vesselID[i], 0, &ThreadId);
			if (vessels[i-1] == NULL)
			{
				fprintf(stderr,"main::Unexpected Error in Vessel %d  Creation\n", i+1);
				return 1;
			}
		}
	}
	else {
		fprintf(stderr, "[%02d:%02d:%02d] Haifa Port: the number %d you entered is prime and not have an approval...\n", local->tm_hour, local->tm_min, local->tm_sec,atoi(argv[1]));
		exit(1);
	}

	while (ReadFile(StdoutRead, message, MAX_STRING, &read, NULL))
	{
		if (strcmp(message, "") != 0) {
			id = atoi(message);
			if (!ReleaseSemaphore(sem[id - 1], 1, NULL))
				fprintf(stderr, "sail_from_eilat::Unexpected error sem[%d].V()\n", atoi(message));
			count++;
			if (count == numvessel)
				break;
		}
	}
	WaitForMultipleObjects(numvessel, vessels, TRUE, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr,"[%02d:%02d:%02d] Haifa Port: All Vessel Threads are done\n", local->tm_hour, local->tm_min, local->tm_sec);
	if (!ReleaseMutex(syncprintmutex))
		printf("Haifa::Unexpected error syncprintmutex.V()\n");
	WaitForSingleObject(pi.hProcess, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Haifa Port: Exiting...\n", local->tm_hour, local->tm_min, local->tm_sec);
	if (!ReleaseMutex(syncprintmutex))
		printf("Haifa::Unexpected error syncprintmutex.V()\n");
	//close all global Semaphore Handles
	cleanupGlobalData();
}

DWORD WINAPI Vessel(PVOID Param)
{
	int vesselID = *(int *)Param;
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr,"[%02d:%02d:%02d] Vessel %d - starts sailing @ Haifa Port\n", local->tm_hour, local->tm_min, local->tm_sec, vesselID);
	if (!ReleaseMutex(syncprintmutex))
		printf("Vessel::Unexpected error syncprintmutex.V()\n");
	Sleep(calcSleepTime());
	sail_to_eilat(vesselID);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr,"[%02d:%02d:%02d] Vessel %d - exiting Canal: Red Sea ==> Med. Sea\n", local->tm_hour, local->tm_min, local->tm_sec, vesselID);
	if (!ReleaseMutex(syncprintmutex))
		printf("Vessel::Unexpected error syncprintmutex.V()\n");
	Sleep(calcSleepTime());
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr,"[%02d:%02d:%02d] Vessel %d - done sailing @ Haifa Port\n", local->tm_hour, local->tm_min, local->tm_sec, vesselID);
	if (!ReleaseMutex(syncprintmutex))
		printf("Vessel::Unexpected error syncprintmutex.V()\n");
	return 0;
}

int calcSleepTime()
{
	srand((unsigned int)time(NULL));
	return(rand() % MAX_SLEEP_TIME + MIN_SLEEP_TIME);
}

void cleanupGlobalData()
{
	int i;

	//CloseHandle(haifasem);
	CloseHandle(printmutex);
	CloseHandle(haifamutex);
	/* close the write end of the pipe */
	CloseHandle(StdinWrite);
	/* close the read end of the pipe */
	CloseHandle(StdoutRead);
	for (i = 0; i < numvessel; i++)
		CloseHandle(sem[i]);
	free(vessels);
	CloseHandle(syncprintmutex);
}
int initGlobalData(int num)
{
	printmutex = CreateMutex(NULL, FALSE, NULL);
	if (printmutex == NULL)
	{
		return False;
	}
	haifamutex = CreateMutex(NULL, FALSE, NULL);
	if (haifamutex == NULL)
	{
		return False;
	}
	for (int i = 0; i < num; i++)
	{
		sem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		if (sem[i] == NULL)
		{
			return False;
		}
	}
	return True;
}

void sail_to_eilat(int id)
{
	WaitForSingleObject(haifamutex, INFINITE);
	char sid[MAX_STRING];
	sprintf(sid, "%d", id);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr,"[%02d:%02d:%02d] Vessel %d - entering Canal: Med. Sea ==> Red Sea\n", local->tm_hour, local->tm_min, local->tm_sec, id);
	if (!ReleaseMutex(syncprintmutex))
		printf("sail_to_eilat::Unexpected error syncprintmutex.V()\n");
	if (!WriteFile(StdinWrite, sid, MAX_STRING, &written, NULL))
		fprintf(stderr, "Error writing to pipe\n");
	if (!ReleaseMutex(haifamutex))
		fprintf(stderr,"sail_to_eilat::Unexpected error mutex.V()\n");
	WaitForSingleObject(sem[id-1], INFINITE);
}


void print()
{
	WaitForSingleObject(syncprintmutex, INFINITE);
	WaitForSingleObject(printmutex, INFINITE);
	time_t now;
	time(&now);
	local = localtime(&now);
	if (!ReleaseMutex(printmutex))
		fprintf(stderr,"print::Unexpected error mutex.V()\n");
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr, "print::Unexpected error syncprintmutex.V()\n");
}

