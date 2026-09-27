#include <mutex>
#include <vector>
#include <algorithm>

#include "Cemu/Logging/CemuLogging.h"
#include "WindowSystem.h"
#include "config/ActiveSettings.h"
#include "util/crypto/aes128.h"
#include "Common/FileStream.h"
#include "util/helpers/StringHelpers.h"
#include "util/helpers/helpers.h"

std::mutex mtxKeyCache;

struct KeyCacheEntry
{
	uint8 aes128key[16];
};

std::vector<KeyCacheEntry> g_keyCache;

bool strishex(std::string_view str)
{
	for(size_t i=0; i<str.size(); i++)
	{
		char c = str[i];
		if( (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') )
			continue;
		return false;
	}
	return true;
}

/*
 * Returns AES-128 key from the key cache
 * nullptr is returned if index >= max_keys
 */
uint8* KeyCache_GetAES128(sint32 index)
{
	if( index < 0 || index >= (sint32)g_keyCache.size())
		return nullptr;
	KeyCacheEntry* keyCacheEntry = &g_keyCache[index];
	return keyCacheEntry->aes128key;
}

void KeyCache_AddKey128(const uint8* key)
{
	for (const auto& entry : g_keyCache)
	{
		if (memcmp(entry.aes128key, key, 16) == 0)
			return;
	}
	KeyCacheEntry newEntry = {0};
	memcpy(newEntry.aes128key, key, 16);
	g_keyCache.emplace_back(newEntry);
}

bool sKeyCachePrepared = false;

void KeyCache_Prepare()
{
	mtxKeyCache.lock();
	if (sKeyCachePrepared && !g_keyCache.empty())
	{
		mtxKeyCache.unlock();
		return;
	}
	sKeyCachePrepared = true;
	g_keyCache.clear();

	std::vector<fs::path> candidatePaths;
	candidatePaths.push_back(ActiveSettings::GetUserDataPath("keys.txt"));
	candidatePaths.push_back(ActiveSettings::GetUserDataPath().parent_path() / "keys.txt");
	candidatePaths.push_back(ActiveSettings::GetConfigPath("keys.txt"));
	candidatePaths.push_back(ActiveSettings::GetConfigPath().parent_path() / "keys.txt");
	candidatePaths.push_back(ActiveSettings::GetDataPath("keys.txt"));
	candidatePaths.push_back(ActiveSettings::GetDataPath().parent_path() / "keys.txt");
	candidatePaths.push_back(fs::path("/sdcard/Download/keys.txt"));
	candidatePaths.push_back(fs::path("/sdcard/Download/Roms/wiiu/keys.txt"));
	candidatePaths.push_back(fs::path("/sdcard/Download/Roms/keys.txt"));
	candidatePaths.push_back(fs::path("/sdcard/keys.txt"));

	// Deduplicate candidate paths
	std::vector<fs::path> uniquePaths;
	for (const auto& p : candidatePaths)
	{
		if (p.empty()) continue;
		if (std::find(uniquePaths.begin(), uniquePaths.end(), p) == uniquePaths.end())
			uniquePaths.push_back(p);
	}

	bool loadedAny = false;
	for (const auto& keysPath : uniquePaths)
	{
		FileStream* fs_keys = FileStream::openFile2(keysPath);
		if (!fs_keys)
			continue;

		cemuLog_log(LogType::Force, "KeyCache: Reading keys from {}", _pathToUtf8(keysPath));
		sint32 lineNumber = 0;
		std::string line;
		size_t keysInFile = 0;
		while (fs_keys->readLine(line))
		{
			lineNumber++;
			// truncate anything after '#' or ';'
			for (size_t i = 0; i < line.size(); i++)
			{
				if (line[i] == '#' || line[i] == ';')
				{
					line.resize(i);
					break;
				}
			}
			// remove whitespaces and other common formatting characters
			auto itr = line.begin();
			while (itr != line.end())
			{
				char c = *itr;
				if (c == ' ' || c == '\t' || c == '-' || c == '_')
					itr = line.erase(itr);
				else
					itr++;
			}
			if (line.empty())
				continue;
			if (strishex(line) == false)
			{
				cemuLog_log(LogType::Force, "KeyCache: Format error in {} at line {}", _pathToUtf8(keysPath), lineNumber);
				continue;
			}
			if (line.size() == 32)
			{
				// 128-bit key
				uint8 keyData128[16];
				StringHelpers::ParseHexString(line, keyData128, 16);
				KeyCache_AddKey128(keyData128);
				keysInFile++;
			}
		}
		delete fs_keys;
		loadedAny = true;
		cemuLog_log(LogType::Force, "KeyCache: Loaded {} valid key(s) from {}", keysInFile, _pathToUtf8(keysPath));
	}

	if (!loadedAny)
	{
		auto defaultKeysPath = ActiveSettings::GetUserDataPath("keys.txt");
		FileStream* fs_keys = FileStream::createFile2(defaultKeysPath);
		if (fs_keys)
		{
			fs_keys->writeString("# this file contains keys needed for decryption of disc file system data (WUD/WUX)\r\n");
			fs_keys->writeString("# 1 key per line, any text after a '#' character is considered a comment\r\n");
			fs_keys->writeString("# the emulator will automatically pick the right key\r\n");
			fs_keys->writeString("541b9889519b27d363cd21604b97c67a # example key (can be deleted)\r\n");
			delete fs_keys;
			cemuLog_log(LogType::Force, "KeyCache: Created template keys.txt at {}", _pathToUtf8(defaultKeysPath));
		}
	}

	cemuLog_log(LogType::Force, "KeyCache: Total keys cached: {}", g_keyCache.size());
	mtxKeyCache.unlock();
}