#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>
#include <stdlib.h> //for rand/strand
#include <ctype.h>
#include <time.h>    //for using the time as the seed to strand!
#define MAX_SLEEP_TIME 3000 
#define MIN_SLEEP_TIME 5
#define MAX_CARGO 50
#define MIN_CARGO 5
#define True 1
#define False 0
#define BUFFER_SIZE 50
#define N 50
int numvessel=0, *vesselID, random = 0, *craneID,countvessel = 0,*cranevessel, freecranes = 0,barrierindex=0, *vesselinbarrier,countv=0, cvesselinbarrier=0,*cargocrane;
HANDLE eilatmutex,printmutex,craneMutex, cranesem,*vesselsem,unloadmutex,*cranesemaphore, syncprintmutex,cargoran;
HANDLE ReadHandle, WriteHandle;
HANDLE *vessels, *cranes;
CHAR buffer[BUFFER_SIZE];
DWORD WINAPI Vessel(PVOID), read, written, ThreadId;
DWORD WINAPI Crane(PVOID);
BOOL success;
struct tm *local;
time_t t;
int check_prime(int);
int initGlobalData(int,int);
void cleanupGlobalData();
int calcSleepTime();
int crane_num(int);
int cargo_num();
void print();
void sail_to_haifa(int);
void enterBarier(int);
void realese_from_barrier(int);
int enter_unloading(int);
void exit_unloading(int,int);
void unloading(int);
void initdynamicarrays();
int main(VOID)
{
	int prime=0, i=0, count=1;
	char sprime[BUFFER_SIZE];
	ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
	WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	time_t now;
	time(&now);
	local = localtime(&now);
	syncprintmutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("syncprintmutex"));
	if (syncprintmutex == NULL)
	{
		fprintf(stderr, "Eilat::Error create syncprintmutex\n");
	}
	// now have the child read from the pipe
	success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL);

	// we have to output to stderr as stdout is redirected to the pipe
	if (success)
	{
		fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: Get request from Haifa for %d Vessels\n", local->tm_hour, local->tm_min, local->tm_sec, atoi(buffer));
		prime = check_prime(atoi(buffer));
	}
	else 
		fprintf(stderr, "Eilat: error reading from pipe\n");
	if (prime == 1)
	{
		fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: Deny the request from Haifa for %d Vessels\n", local->tm_hour, local->tm_min, local->tm_sec, atoi(buffer));
		exit(1);
	}
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: Approve the request from Haifa for %d Vessels\n", local->tm_hour, local->tm_min, local->tm_sec, atoi(buffer));
	sprintf(sprime, "%d", prime);
	// now write amended string  to the pipe
	if (!WriteFile(WriteHandle, sprime, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "Eilat::Error writing to pipe\n");
	numvessel = atoi(buffer);
	random = crane_num(numvessel - 1);
	freecranes = random;
	initdynamicarrays();
	if (initGlobalData(numvessel, random) == False) {
		fprintf(stderr, "Eilat::Unexpected Error in Global Semaphore Creation\n");
		return 1;
	}
	for (int i = 0; i < random; i++)
	{
		craneID[i] = i+1;
		cranes[i] = CreateThread(NULL, 0, Crane, &craneID[i], 0, &ThreadId);
		if (cranes[i] == NULL)
		{
			fprintf(stderr, "Eilat::Unexpected Error in Crane %d Creation\n", i);
			return 1;
		}
	}
	while(ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL))
	{
		if (strcmp(buffer,"")!=0) {
			vesselID[i] = atoi(buffer);
			vessels[i] = CreateThread(NULL, 0, Vessel, &vesselID[i], 0, &ThreadId);
			if (vessels[i] == NULL)
			{
				fprintf(stderr, "Eilat::Unexpected Error in Vessel %d  Creation\n", vesselID[i]);
			}
			i++;
			if (i == numvessel)
				break;
		}
	}
	Sleep(calcSleepTime());
	WaitForMultipleObjects(numvessel, vessels, TRUE, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: All Vessel Threads are done\n", local->tm_hour, local->tm_min, local->tm_sec);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr, "Eilat::Unexpected error syncprintmutex.V()\n");
	WaitForMultipleObjects(random, cranes, TRUE, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: All Crane Threads are done\n", local->tm_hour, local->tm_min, local->tm_sec);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr, "Eilat::Unexpected error syncprintmutex.V()\n");
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: Exiting...\n", local->tm_hour, local->tm_min, local->tm_sec);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr, "Eilat::Unexpected error syncprintmutex.V()\n");
	return 0;
}

int check_prime(int p) 
{
	int num;
	for (num = 2; num <=p-1; num++)
	{
		if (p%num == 0)
			return 0;
	}
	if (num == p)
		return 1;
}

void sail_to_haifa(int id)
{
	WaitForSingleObject(eilatmutex, INFINITE);
	char sid[BUFFER_SIZE];
	sprintf(sid, "%d", id);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - entering Canal: Red Sea ==> Med. Sea\n", local->tm_hour, local->tm_min, local->tm_sec, id);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"sail_to_haifa::Unexpected error syncprintmutex.V()\n");
	if (!WriteFile(WriteHandle, sid, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "sail_to_haifa::Error writing to pipe\n");
	if (!ReleaseMutex(eilatmutex))
		fprintf(stderr,"sail_to_haifa::Unexpected error mutex.V()\n");
}

DWORD WINAPI Vessel(PVOID Param)
{
	int vesselID = *(int *)Param, crane=0,cargo=0;
	Sleep(calcSleepTime());
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - arrived @ Eilat Port\n", local->tm_hour, local->tm_min, local->tm_sec, vesselID);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"Eilat Vessel::Unexpected error syncprintmutex.V()\n");
	Sleep(calcSleepTime());
	enterBarier(vesselID);
	Sleep(calcSleepTime());
	crane = enter_unloading(vesselID);
	Sleep(calcSleepTime());
	exit_unloading(vesselID,crane);
	Sleep(calcSleepTime());
	sail_to_haifa(vesselID);
	return 0;
}
DWORD WINAPI Crane(PVOID Param)
{
	int craneID = *(int *)Param;
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Crane  %d - Wait for Unload...\n", local->tm_hour, local->tm_min, local->tm_sec, craneID);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"Crane::Unexpected error syncprintmutex.V()\n");
	while (countvessel < numvessel)
	{
		Sleep(calcSleepTime());
		unloading(craneID);
		Sleep(calcSleepTime());

	}
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Crane  %d - Finish Unload...\n", local->tm_hour, local->tm_min, local->tm_sec, craneID);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"Eilat Vessel::Unexpected error syncprintmutex.V()\n");
	return 0;
}
int calcSleepTime()
{
	srand((unsigned int)time(NULL));
	return(rand() % MAX_SLEEP_TIME + MIN_SLEEP_TIME);
}
int crane_num(int num)
{
	int div = 0;
	srand((unsigned int)time(NULL));
	do {
		div = rand() % num + 1;
	} while ((numvessel % div) != 0);
	return div;
}
int cargo_num()
{
	int c;
	Sleep(1000);
	srand((unsigned int)time(NULL));
	c = rand() % MAX_CARGO + MIN_CARGO;
	return c;
}
void print()
{
	WaitForSingleObject(printmutex, INFINITE);
	time_t now;
	time(&now);
	local = localtime(&now);
	if (!ReleaseMutex(printmutex))
		fprintf(stderr,"Print::Unexpected error mutex.V()\n");
}

int initGlobalData(int num, int crane)
{
	eilatmutex = CreateMutex(NULL, FALSE, NULL);
	if (eilatmutex == NULL)
	{
		return False;
	}
	printmutex = CreateMutex(NULL, FALSE, NULL);
	if (printmutex == NULL)
	{
		return False;
	}
	craneMutex = CreateMutex(NULL, FALSE, NULL);
	if(craneMutex == NULL)
	{
		return False;
	}
	cranesem = CreateSemaphore(NULL, crane, crane, NULL);
	if(cranesem ==NULL)
	{
		return False;
	}
	unloadmutex = CreateMutex(NULL, FALSE, NULL);
	if (unloadmutex == NULL)
	{
		return False;
	}
	for (int i = 0; i < num; i++)
	{
		vesselsem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		if (vesselsem[i] == NULL)
		{
			return False;
		}
	}
	for (int i = 0; i < crane; i++)
	{
		cranesemaphore[i] = CreateSemaphore(NULL, 0, 1, NULL);
		if (vesselsem[i] == NULL)
		{
			return False;
		}
	}
	for (int i = 0; i < crane; i++)
	{
		cranevessel[i] = -1;
		cargocrane[i] = -1;
	}
	return True;
}
void cleanupGlobalData()
{
	int i;
	CloseHandle(eilatmutex);
	CloseHandle(printmutex);
	CloseHandle(craneMutex);
	for (i = 0; i < numvessel; i++)
		CloseHandle(vesselsem[i]);
	for (i = 0; i < numvessel; i++)
		CloseHandle(cranesemaphore[i]);
	CloseHandle(cranesem);
	CloseHandle(unloadmutex);
	CloseHandle(syncprintmutex);
	CloseHandle(ReadHandle);
	CloseHandle(WriteHandle);
	free(vessels);
	free(vesselID);
	free(craneID);
	free(cranevessel);
	free(vesselinbarrier);
	free(cargocrane);
}
void enterBarier(int id)
{
	WaitForSingleObject(craneMutex, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - Enter to the Barier\n", local->tm_hour, local->tm_min, local->tm_sec, id);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"enterBarier::Unexpected error syncprintmutex.V()\n");
	vesselinbarrier[barrierindex] = id;
	barrierindex++;
	cvesselinbarrier++;
	realese_from_barrier(id);
	if (!ReleaseMutex(craneMutex))
		fprintf(stderr,"enterBarier::Unexpected error mutex.V()\n");
	WaitForSingleObject(vesselsem[id-1], INFINITE);
}
void realese_from_barrier(int id)
{
	if ((freecranes == random) && (cvesselinbarrier >= random))
	{
		for (int i = 0; i < random; i++)
		{
			cvesselinbarrier--;
			freecranes--;
			if (!ReleaseSemaphore(vesselsem[vesselinbarrier[countv]-1], 1, NULL))
				fprintf(stderr,"realese_from_barrier::Unexpected error mutex.V()\n");
			print();
			WaitForSingleObject(syncprintmutex, INFINITE);
			fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - Exit from the Barier\n", local->tm_hour, local->tm_min, local->tm_sec, vesselinbarrier[countv]);
			if (!ReleaseMutex(syncprintmutex))
				fprintf(stderr,"realese_from_barrier::Unexpected error syncprintmutex.V()\n");
			countv++;
		}
	}
}
int enter_unloading(int id)
{
	int available,cargo = 0;
	WaitForSingleObject(cranesem, INFINITE);
	WaitForSingleObject(unloadmutex, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d -  Enter Unloading\n", local->tm_hour, local->tm_min, local->tm_sec, id);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr, "enter_unloading::Unexpected error syncprintmutex.V()\n");
	Sleep(calcSleepTime());
	for (available = 0; available < random; available++)
	{
		if (cranevessel[available] == -1)
		{
			cranevessel[available] = id;
			break;
		}
	}
	countvessel++;
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	cargo = cargo_num();
	cargocrane[available] = cargo;
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - Unload Cargo By Crane %d and my Cargo is %d\n", local->tm_hour, local->tm_min, local->tm_sec, id, available+1, cargocrane[available]);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"enter_unloading::Unexpected error syncprintmutex.V()\n");
	if (!ReleaseMutex(unloadmutex))
		fprintf(stderr,"enter_unloading::Unexpected error mutex.V()\n");
	if (!ReleaseSemaphore(cranesemaphore[available], 1, NULL))
		fprintf(stderr,"enter_unloading::Unexpected error mutex.V()\n");
	WaitForSingleObject(vesselsem[id - 1], INFINITE);
	return available;
}
void exit_unloading(int id,int crane)
{
	WaitForSingleObject(unloadmutex, INFINITE);
	freecranes++;
	cranevessel[crane] = -1;
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Vessel %d - Exit from the Unloading\n", local->tm_hour, local->tm_min, local->tm_sec, id);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"exit_unloading::Unexpected error syncprintmutex.V()\n");
	realese_from_barrier(id);
	if (!ReleaseMutex(unloadmutex))
		fprintf(stderr,"exit_unloading::Unexpected error mutex.V()\n");
	if (!ReleaseSemaphore(cranesem, 1, NULL));
		printf("exit_unloading::Unexpected error semaphore.V()\n");
}
void unloading(int id)
{
	WaitForSingleObject(cranesemaphore[id - 1], INFINITE);
	WaitForSingleObject(unloadmutex, INFINITE);
	print();
	WaitForSingleObject(syncprintmutex, INFINITE);
	fprintf(stderr, "[%02d:%02d:%02d] Crane %d - Unload Cargo Weight %d\n", local->tm_hour, local->tm_min, local->tm_sec, id, cargocrane[id - 1]);
	if (!ReleaseMutex(syncprintmutex))
		fprintf(stderr,"unloading::Unexpected error syncprintmutex.V()\n");
	cargocrane[id - 1] = -1;
	if (!ReleaseMutex(unloadmutex))
		fprintf(stderr,"unloading::Unexpected error mutex.V()\n");
	if (!ReleaseSemaphore(vesselsem[cranevessel[id-1]-1], 1, NULL))
		fprintf(stderr,"enter_unloading::Unexpected error mutex.V()\n");
}
void initdynamicarrays()
{
	vessels = calloc(numvessel, sizeof(HANDLE));
	vesselID = (int *)malloc(numvessel * sizeof(int));
	vesselinbarrier = (int *)malloc(numvessel * sizeof(int));
	cranevessel = (int *)malloc(random * sizeof(int));
	cranes = calloc(random, sizeof(HANDLE));
	craneID = calloc(random, sizeof(HANDLE));
	vesselsem = calloc(numvessel, sizeof(HANDLE));
	cranesemaphore = calloc(random, sizeof(HANDLE));
	cranesem = calloc(random, sizeof(HANDLE));
	cargocrane = (int *)malloc(random * sizeof(int));
}
