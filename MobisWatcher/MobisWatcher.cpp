// MobisWatcher.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <Windows.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <process.h>
#include <string>
#include <atlconv.h>
#include "../P7/P7_Trace.h"

//TODO Оформить все с файлом настроек, чтобы получился универсальный наблюдатель и задеплоить в ГитХаб

IP7_Client			*l_pClient = NULL;
IP7_Trace			*l_pTrace = NULL;
IP7_Trace::hModule	 l_hModule = NULL;
HANDLE				 hConsole;
std::chrono::minutes timeout(2);
int					hungTime = 60000;
bool				running = true;

std::wstring				BlockWindowP;
std::wstring				ChildBlockWindowP;
std::wstring				TargetWindowP;
std::wstring				LaunchP;
std::wstring				KillProcessP;

const std::wstring	CHILD_BLOCK_WINDOW_PARAM = _T("ChildBlockWindow");
const std::wstring	BLOCK_WINDOW_PARAM = _T("BlockWindow");
const std::wstring	TARGET_WINDOW_PARAM = _T("TargetWindow");
const std::wstring	LAUNCH_PARAM = _T("Launch");
const std::wstring	KILL_PROCESS_PARAM = _T("KillProcess");

using namespace std;
using namespace std::chrono;

void restartMobisEx();
void kill1CEx();
bool isWindowIsHang(HWND, PBOOL);
void processConfigLine(std::wstring, std::wstring[]);

int main()
{
#pragma region P7 logging init
	P7_Set_Crash_Handler();
	l_pClient = P7_Create_Client(TM("/P7.Sink=FileTxt /P7.Format=\"[%tf] %lv %ms\" /P7.Roll=1mb /P7.Files=3 /P7.Pool=2048"));
	if (l_pClient == NULL) {
		std::cout << "Error init P7 log system! Aborting...";
		return -1;
	}
	l_pTrace = P7_Create_Trace(l_pClient, TM("Trace 1"));
	if (l_pTrace == NULL)
	{
		std::cout << "Error creating trace channel! Aborting...";
		return -1;
	}
	l_pTrace->Register_Thread(TM("MobisWatcher"), 0);
	l_pTrace->Register_Module(TM("Main"), &l_hModule);
#pragma endregion

#pragma region Read settings
	std::wstring line;
	std::wifstream in("settings.cfg");
	std::wstring KeyVal[2];
	if (in.is_open())
		while (getline(in, line)) 
		{
			processConfigLine(line, KeyVal);
			if (KeyVal[0].length() > 0) {
				if (KeyVal[0] == BLOCK_WINDOW_PARAM)
					BlockWindowP = KeyVal[1];

				if (KeyVal[0] == CHILD_BLOCK_WINDOW_PARAM)
					ChildBlockWindowP = KeyVal[1];

				if (KeyVal[0] == TARGET_WINDOW_PARAM)
					TargetWindowP = KeyVal[1];

				if (KeyVal[0] == LAUNCH_PARAM)
					LaunchP = KeyVal[1];

				if (KeyVal[0] == KILL_PROCESS_PARAM)
					KillProcessP = KeyVal[1];
			}
		}
	else {
		l_pTrace->P7_ERROR(l_hModule, TM("No settings.cfg found!"));
		cout << "Not Found settings.cfg!" << endl;
		std::wofstream fout;
		fout.open("settings.cfg");
		fout << "BlockWindow =" << endl << "ChildBlockWindow=" << endl << "TargetWindow=V8TopLevelFrame" << endl << "Launch=" << endl << "KillProcess=1cv8.exe";
		fout.close();
		running = false;
	}
#pragma endregion

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	l_pTrace->P7_INFO(l_hModule, TM("Starting watch..."));
	SetConsoleTextAttribute(hConsole, 1);
	cout << R"(    __   ___        __     _   _____   _       __        __         __)" << endl;
	cout << R"(   /  |/   /____   / /_   (_) / ___/  | |     / /____ _ / /_ _____ / /_  )" << endl;
	cout << R"(  / /|_/  // __ \ / __ \ / /  \__\    | | /| / // __ `// __// ___// __ \ )" << endl;
	cout << R"( / /  /  // /_/ // /_/ // / ___/ /    | |/ |/ // /_/ // /_ / /__ / / / /)" << endl;
	cout << R"(/_/  /_ / \____//_.___//_/ /____/     |__/|__/ \__,_/ \__/ \___//_/ /_/ )" << endl;
	SetConsoleTextAttribute(hConsole, 15);
	cout << "Starting watch... (v1.5)" << endl;
	while (running) { //starting main enless loop
		//Find app
		SetConsoleTextAttribute(hConsole, 15);
		cout << "Status check... ";
		HWND blockWindow = FindWindow(BlockWindowP.data(), NULL);
		HWND licenseWindow = FindWindow(NULL, ChildBlockWindowP.data());
		if (blockWindow != NULL || licenseWindow != NULL) {
			SetConsoleTextAttribute(hConsole, 14);
			cout << endl << "Detected blocking window. Something not right! Restart 1C pending.";
			l_pTrace->P7_WARNING(l_hModule, TM("Block window detected. Possible error message showing. 1c will be restarted."));
			kill1CEx();
			restartMobisEx();
		}
		else {
			HWND window = FindWindow(TargetWindowP.data(), NULL);
			if (window == NULL) {
				l_pTrace->P7_WARNING(l_hModule, TM("Not found %s window, app not active?"), TargetWindowP);
				SetConsoleTextAttribute(hConsole, 14);
				cout << "WARNING 1C not found, possible closed" << endl;
				restartMobisEx();
			}
			else {
				PBOOL isHung = new BOOL;
				isWindowIsHang(window, isHung);
				if (*isHung == 1) {
					SetConsoleTextAttribute(hConsole, 12);
					cout << " HUNG DETECTED!!!" << endl;
					l_pTrace->P7_WARNING(l_hModule, TM("Hung detected!!!"));
					kill1CEx();
					restartMobisEx();
				}
				else {
					SetConsoleTextAttribute(hConsole, 10);
					cout << " OK!" << endl;
				}
			}
		}
		P7_Flush();
		std::this_thread::sleep_for(timeout);
	}
#pragma region Ending
	//EXIT
	l_pTrace->Unregister_Thread(0);
	if (l_pTrace) {
		l_pTrace->Release();
		l_pTrace = NULL;
	}
	if (l_pClient) {
		l_pClient->Release();
		l_pClient = NULL;
	}
	return 0;
#pragma endregion
}

bool isWindowIsHang(IN HWND hWnd, OUT PBOOL pbHung) {
	*pbHung = FALSE;
	DWORD_PTR dwResult;
	if (!SendMessageTimeout(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, hungTime, &dwResult)) {
		DWORD res = GetLastError();
		cout << "GetLastError result: " << res;
		if ((res == ERROR_TIMEOUT) || (res = 0)) {
			*pbHung = TRUE;
			return true;
		}
	}
	return false;
}
void restartMobisEx() {
	SetConsoleTextAttribute(hConsole, 15);
	cout << endl << "Restarting... " << endl;
	l_pTrace->P7_INFO(l_hModule, TM("Restarting %s... (Ex)"), LaunchP);
	//TCHAR szPath[] = TEXT("C:\\Progra~1\\1cv8\\8.3.17.1549\\bin\\1cv8.exe ENTERPRISE /IBConnectionString\"Srvr = \"\"SRV-1C-1\"\"; Ref=\"\"UT\"\"\" /N\"Mobi-C\" /P\"mobis\"");
	TCHAR *szPath = (wchar_t *)LaunchP.c_str();
	STARTUPINFO si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	BOOL retOK = CreateProcess(NULL, szPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	if (!retOK) {
		cout << "Restarting... FAILED" << endl;
		l_pTrace->P7_ERROR(l_hModule, TM("Restarting... (Ex) FAILED."));
	}
}

void kill1CEx() {
	SetConsoleTextAttribute(hConsole, 15);
	HANDLE hProcessSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
	PROCESSENTRY32 ProcessEntry = { 0 };
	ProcessEntry.dwSize = sizeof(ProcessEntry);
	cout << "Looking for 1C... " << endl;
	l_pTrace->P7_INFO(l_hModule, TM("Killing %s... (Ex)"), KillProcessP);
	l_pTrace->P7_INFO(l_hModule, TM("Starting process lookup..."));
	BOOL Return = FALSE;
	while (!Return) {
		Return = Process32First(hProcessSnapShot, &ProcessEntry);
	}
	do
	{
		int value = _tcsicmp(ProcessEntry.szExeFile, KillProcessP.data());
		if (value == 0)
		{
			cout << " Found bustard!!! Killing... " << endl;
			l_pTrace->P7_INFO(l_hModule, TM("Process found, Killing."));
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessEntry.th32ProcessID);
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}

	} while (Process32Next(hProcessSnapShot, &ProcessEntry));

	CloseHandle(hProcessSnapShot);
}

void processConfigLine(std::wstring line, std::wstring KeyVal[]) 
{
	KeyVal[0] = _T("");
	KeyVal[1] = _T("");

	if (line.at(0) == '#')
		return; //This line is commented

	//split out comment
	std::string::size_type pos = line.find('#');
	if (pos != std::string::npos)
		line = line.substr(0, pos);
	//split Key-Val pair
	pos = line.find('=');
	if (pos != std::string::npos) {
		KeyVal[0] = line.substr(0, pos);
		KeyVal[1] = line.substr(pos + 1, line.size() - pos);
		pos = KeyVal[0].find_last_not_of(_T(" \n\r\t"))+1;
		KeyVal[0] = KeyVal[0].erase(pos);
		pos = KeyVal[1].find_last_not_of(_T(" \n\r\t"))+1;
		KeyVal[1] = KeyVal[1].erase(pos);
	}
}