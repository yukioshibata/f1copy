#ifndef SCANCODEMAP_H
#define SCANCODEMAP_H

class ScancodeMap {
public:
    // Write our remappings into HKLM Scancode Map (idempotent, safe to call multiple times).
    // Requires administrator privileges.
    // Returns true on success.  A system reboot is required for changes to take effect.
    static bool Install();

    // Remove only our remappings from HKLM Scancode Map.
    // Other applications' remappings are preserved.
    // Requires administrator privileges.
    static void Uninstall();

    // Returns true if our remappings are currently present in the registry.
    static bool IsInstalled();
};

#endif
