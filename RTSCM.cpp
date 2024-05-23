#include <Windows.h>
#include <iostream>
#include <vector>

/// <summary>
/// Stores all registry key handles that should be destroyed later
/// </summary>
std::vector<HKEY> handles;

/// <summary>
/// Destroys all registry key handles in the list above
/// </summary>
/// <returns>Successful status</returns>
int destroyHandles() {
	for (HKEY handle : handles)
		RegCloseKey(handle);
	handles.clear();
	return 0;
}

/// <summary>
/// Opens an existing registry key
/// </summary>
/// <param name="base">Parent registry key handle</param>
/// <param name="key">Key to open</param>
/// <param name="hKey">Handle to the opened key</param>
/// <returns>True on success, false if the key doesn't exist</returns>
bool openKey(HKEY base, const char* key, HKEY* hKey) {
	LSTATUS status = RegOpenKeyExA(base, key, NULL, KEY_READ | KEY_WOW64_64KEY, hKey);
	if (!status) {
		handles.push_back(*hKey);
		return true;
	}
	return false;
}

/// <summary>
/// Creates a new registry key
/// </summary>
/// <param name="base">Parent registry key handle</param>
/// <param name="key">Key to create</param>
/// <param name="hKey">Handle to the new key</param>
/// <returns>True on success, false if the key already exists</returns>
bool createKey(HKEY base, const char* key, HKEY* hKey) {
	LSTATUS status = RegCreateKeyExA(base, key, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, hKey, NULL);
	if (!status) {
		handles.push_back(*hKey);
		return true;
	}
	return false;
}

/// <summary>
/// Sets the value of a registry key
/// </summary>
/// <param name="base">Parent registry key handle</param>
/// <param name="key">Key to set</param>
/// <param name="value">Value to set</param>
/// <returns>True on success</returns>
bool setKey(HKEY base, const char* key, const char* value) {
	return !RegSetKeyValueA(base, key, NULL, REG_SZ, value, (DWORD)strlen(value) + 1);
}

/// <summary>
/// Renames a registry key
/// </summary>
/// <param name="base">Parent registry key handle</param>
/// <param name="key">Key to rename</param>
/// <param name="newkey">New key name</param>
/// <returns>True on success</returns>
bool renameKey(HKEY base, const char* key, const char* newkey) {
	std::string temp = key;
	std::wstring oldk(temp.begin(), temp.end());
	temp = newkey;
	std::wstring newk(temp.begin(), temp.end());
	return !RegRenameKey(base, oldk.c_str(), newk.c_str());
}

/// <summary>
/// Shows an error message and safely closes all handles
/// </summary>
/// <param name="code">Exit code</param>
/// <returns>Exit code</returns>
int fail(int code) {
	printf("Fail!\n");
	destroyHandles();
	system("pause");
	return code;
}

/// <summary>
/// Main entrypoint of the program
/// </summary>
/// <returns>Exit code</returns>
int main() {
	//Registry key handles that we might need later
	HKEY software, classes, clsid, something, inprocserver32;
	//True if the context menu was changed to windows 10
	bool changed = false;
	printf("Remove (or restore) that shitty Context Menu\n");

	//Get all registry key handles that we need to reach
	//HKEY_CURRENT_USER/Software/Classes/CLSID
	printf("Retrieving Software Key...\n");
	if (!openKey(HKEY_CURRENT_USER, "Software", &software))
		return fail(-1);
	printf("Success! 0x%p\n\n", software);

	printf("Retrieving Classes Key...\n");
	if (!openKey(software, "Classes", &classes))
		return fail(-2);
	printf("Success! 0x%p\n\n", classes);

	printf("Retrieving CLSID Key...\n");
	if (!openKey(classes, "CLSID", &clsid))
		return fail(-3);
	printf("Success! 0x%p\n\n", clsid);

	//Check if our patch was already applied
	printf("Checking if InprocServer32 GUID exists...\n");
	if (!openKey(clsid, "{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}", &something)) {
		//Patch wasn't applied, in that case
		//create a GUID for our InprocServer32
		//key, otherwise we can move on
		printf("InprocServer32 GUID doesn't exist, creating it...\n");
		if (!createKey(clsid, "{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}", &something))
			return fail(-4);
		printf("Success! 0x%p\n\n", something);
	}
	else printf("InprocServer32 GUID found! 0x%p\n\n", something);

	//First, check if the InprocServer32 registry key exists
	printf("Checking if InprocServer32 Key exists...\n");
	if (!openKey(something, "InprocServer32", &inprocserver32)) {
		//If not, check if our renamed key exists
		if (!openKey(something, "InprocServer31", &inprocserver32)) {
			//If not, create the InprocServer32 key
			printf("InprocServer32 Key doesn't exist, creating it...\n");
			if (!createKey(something, "InprocServer32", &inprocserver32))
				return fail(-5);
			printf("Success! 0x%p\n\n", inprocserver32);

			//We need to set any value to it, for example an empty string
			printf("Setting InprocServer32 Value...\n");
			if (!setKey(something, "InprocServer32", ""))
				return fail(-8);
			printf("Success!\n\n");
		}
		else {
			//If yes, we can restore our patch by renaming the existing key
			printf("Alternative Key found, renaming it...\n");
			if (!renameKey(something, "InprocServer31", "InprocServer32"))
				return fail(-6);
			if (!openKey(something, "InprocServer32", &inprocserver32))
				return fail(-7);
			printf("Success! 0x%p\n\n", inprocserver32);
		}

		changed = true;
	}
	else {
		//If yes, we remove our patch by renaming it to something different
		printf("InprocServer32 Key found, removing it...\n");
		if (!renameKey(something, "InprocServer32", "InprocServer31"))
			return fail(-9);
		printf("Success!\n\n");
	}

	//We need to restart the explorer to apply the changes,
	//this can be done by getting a process handle...
	printf("Retrieving Explorer Handle...\n");
	HWND explorerWnd = FindWindowA("Shell_TrayWnd", NULL);
	DWORD explorerPid = NULL;
	GetWindowThreadProcessId(explorerWnd, &explorerPid);
	HANDLE hExplorer = OpenProcess(PROCESS_ALL_ACCESS, FALSE, explorerPid);
	printf("Success! 0x%p\n\n", hExplorer);

	//...and terminating the process with status 2
	//This will automatically restart the process
	//so we don't have to use CreateProcessA
	printf("Restarting Explorer Process...\n");
	TerminateProcess(hExplorer, 2);
	CloseHandle(hExplorer);
	printf("Success!\n\n");

	//Let the user know which context menu they have now
	if (changed)
		printf("Successfully installed Windows 10 Context Menu!\n");
	else
		printf("Successfully restored Windows 11 Context Menu!\n");

	//Exit and destroy all handles
	system("pause");
	return destroyHandles();
}