#include "ScancodeMap.h"
#include <windows.h>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------------------
// Registry location for Scancode Map
// ---------------------------------------------------------------------------
static const wchar_t* REG_KEY  = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
static const wchar_t* REG_VAL  = L"Scancode Map";

// ---------------------------------------------------------------------------
// Our remappings:
//   CapsLock  (scan 0x003A) --> Left Ctrl (scan 0x001D)
//   ScrollLock(scan 0x0046) --> CapsLock  (scan 0x003A)
//
// Entry format (little-endian WORDs packed into a DWORD):
//   high WORD = source scancode, low WORD = target scancode
// ---------------------------------------------------------------------------
struct ScEntry {
    WORD target; // "to"
    WORD source; // "from"
};

static const ScEntry k_OurEntries[] = {
    { 0x001D, 0x003A },  // CapsLock  -> LCtrl
    { 0x003A, 0x0046 },  // ScrollLock-> CapsLock
};
static const int k_EntryCount = (int)(sizeof(k_OurEntries) / sizeof(k_OurEntries[0]));

// ---------------------------------------------------------------------------
// Low-level helpers to parse / build the binary Scancode Map blob
//
// Binary layout:
//   [0x00000000]      DWORD version  = 0
//   [0x00000000]      DWORD flags    = 0
//   [count]           DWORD N        = number of entries INCLUDING terminator
//   [to:WORD,from:WORD] ...          N-1 remapping entries
//   [0x00000000]      DWORD          terminator
// ---------------------------------------------------------------------------

// Parse the REG_BINARY blob into a list of ScEntry (excluding the terminator).
static std::vector<ScEntry> ParseBlob(const std::vector<BYTE>& blob) {
    std::vector<ScEntry> entries;
    // Minimum: 8 bytes header + 4 bytes count + 4 bytes terminator = 16 bytes
    if (blob.size() < 16) return entries;

    DWORD count = 0;
    memcpy(&count, blob.data() + 8, sizeof(DWORD));
    if (count < 1) return entries;

    size_t numEntries = count - 1; // exclude terminator
    size_t needed = 12 + numEntries * 4; // 8 header + 4 count + entries + 4 term
    if (blob.size() < needed) return entries;

    for (size_t i = 0; i < numEntries; ++i) {
        ScEntry e;
        memcpy(&e, blob.data() + 12 + i * 4, sizeof(ScEntry));
        if (e.target != 0 || e.source != 0) // skip null entries
            entries.push_back(e);
    }
    return entries;
}

// Build a REG_BINARY blob from a list of ScEntry.
static std::vector<BYTE> BuildBlob(const std::vector<ScEntry>& entries) {
    DWORD count = (DWORD)(entries.size() + 1); // +1 for terminator
    std::vector<BYTE> blob(8 + 4 + entries.size() * 4 + 4, 0);
    // Header (version=0, flags=0) is already zeroed.
    memcpy(blob.data() + 8, &count, sizeof(DWORD));
    for (size_t i = 0; i < entries.size(); ++i)
        memcpy(blob.data() + 12 + i * 4, &entries[i], sizeof(ScEntry));
    // Terminator is already zeroed.
    return blob;
}

// Read the current Scancode Map from the registry.
// Returns an empty vector if the key/value does not exist.
static std::vector<BYTE> ReadRegistry() {
    std::vector<BYTE> blob;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return blob;

    DWORD type = 0, size = 0;
    if (RegQueryValueExW(hKey, REG_VAL, NULL, &type, NULL, &size) == ERROR_SUCCESS
        && type == REG_BINARY && size > 0) {
        blob.resize(size);
        RegQueryValueExW(hKey, REG_VAL, NULL, &type, blob.data(), &size);
    }
    RegCloseKey(hKey);
    return blob;
}

// Write a new blob to the registry (requires admin rights).
static bool WriteRegistry(const std::vector<BYTE>& blob) {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    res = RegSetValueExW(hKey, REG_VAL, 0, REG_BINARY, blob.data(), (DWORD)blob.size());
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

// Delete the registry value entirely (when all entries have been removed).
static void DeleteRegistry() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, REG_VAL);
        RegCloseKey(hKey);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ScancodeMap::Install() {
    // 1. Read current map.
    auto blob    = ReadRegistry();
    auto entries = ParseBlob(blob);

    // 2. Remove any existing entries whose source matches one we own
    //    (prevents duplicates and overwrites conflicting third-party entries).
    for (const auto& ours : k_OurEntries) {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const ScEntry& e) { return e.source == ours.source; }),
            entries.end());
    }

    // 3. Append our entries.
    for (const auto& e : k_OurEntries)
        entries.push_back(e);

    // 4. Write back.
    return WriteRegistry(BuildBlob(entries));
}

void ScancodeMap::Uninstall() {
    auto blob    = ReadRegistry();
    auto entries = ParseBlob(blob);

    // Remove only our entries (matched by both source AND target).
    for (const auto& ours : k_OurEntries) {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const ScEntry& e) {
                    return e.source == ours.source && e.target == ours.target;
                }),
            entries.end());
    }

    if (entries.empty())
        DeleteRegistry();       // No remappings left: remove the value entirely.
    else
        WriteRegistry(BuildBlob(entries));
}

bool ScancodeMap::IsInstalled() {
    auto blob    = ReadRegistry();
    auto entries = ParseBlob(blob);

    for (const auto& ours : k_OurEntries) {
        bool found = false;
        for (const auto& e : entries)
            if (e.source == ours.source && e.target == ours.target) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
