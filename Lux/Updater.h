#pragma once
#include "Common.h"

struct UpdateCheckResult {
    bool success = false;
    bool updateAvailable = false;
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring releaseNotes;
    std::wstring errorMessage;
};

class Updater {
public:
    // Comprueba de forma síncrona si hay una nueva versión en GitHub
    static UpdateCheckResult CheckForUpdate();

    // Descarga, reemplaza el ejecutable actual y reinicia la aplicación
    static bool ApplyUpdate(const std::wstring& downloadUrl, std::wstring& outError);

    // Limpia archivos temporales o copias previas de actualización (.old)
    static void CleanOldFiles();

private:
    static bool IsNewerVersion(const std::wstring& latest, const std::wstring& current);
    static std::string FetchUrl(const std::wstring& url);
    static std::string ExtractJsonString(const std::string& json, const std::string& key);
};
