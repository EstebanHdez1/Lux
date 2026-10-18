#include "Updater.h"
#include "Utils.h"
#include <sstream>

std::string Updater::FetchUrl(const std::wstring& url) {
    HINTERNET hInternet = InternetOpenW(L"Lux-Updater", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return "";

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE;
    const wchar_t* headers = L"User-Agent: Lux-App\r\nAccept: application/vnd.github.v3+json\r\n";

    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), headers, -1, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, nullptr)) {
        if (statusCode == 404) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return "404";
        }
        if (statusCode != 200) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return "";
        }
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return response;
}

std::string Updater::ExtractJsonString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + pattern.length());
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";

    size_t endPos = json.find('"', pos + 1);
    if (endPos == std::string::npos) return "";

    return json.substr(pos + 1, endPos - pos - 1);
}

bool Updater::IsNewerVersion(const std::wstring& latest, const std::wstring& current) {
    auto parseVersion = [](std::wstring v) -> std::vector<int> {
        if (!v.empty() && (v[0] == L'v' || v[0] == L'V')) {
            v = v.substr(1);
        }
        std::vector<int> parts;
        std::wstringstream ss(v);
        std::wstring item;
        while (std::getline(ss, item, L'.')) {
            try {
                parts.push_back(std::stoi(item));
            } catch (...) {
                parts.push_back(0);
            }
        }
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    };

    std::vector<int> vLatest = parseVersion(latest);
    std::vector<int> vCurrent = parseVersion(current);

    for (size_t i = 0; i < 3; ++i) {
        if (vLatest[i] > vCurrent[i]) return true;
        if (vLatest[i] < vCurrent[i]) return false;
    }
    return false;
}

UpdateCheckResult Updater::CheckForUpdate() {
    UpdateCheckResult result;

    std::string json = FetchUrl(GITHUB_API_LATEST);
    if (json == "404") {
        // No hay releases creados aún en el repositorio; el usuario está en la versión inicial
        result.success = true;
        result.updateAvailable = false;
        result.latestVersion = APP_VERSION;
        return result;
    }

    if (json.empty()) {
        result.success = false;
        result.errorMessage = L"No se pudo conectar con el servidor de GitHub.";
        return result;
    }

    std::string tag = ExtractJsonString(json, "tag_name");
    if (tag.empty()) {
        result.success = false;
        result.errorMessage = L"Respuesta no válida del repositorio.";
        return result;
    }

    std::wstring latestVer(tag.begin(), tag.end());
    result.latestVersion = latestVer;

    // Buscar la URL de descarga del ejecutable Lux.exe en los assets
    std::string downloadUrlStr;
    size_t exePos = json.find(".exe");
    if (exePos != std::string::npos) {
        // Retroceder para buscar el inicio de browser_download_url
        size_t urlTagPos = json.rfind("\"browser_download_url\"", exePos);
        if (urlTagPos != std::string::npos) {
            size_t valStart = json.find("https://", urlTagPos);
            if (valStart != std::string::npos && valStart < exePos) {
                downloadUrlStr = json.substr(valStart, (exePos + 4) - valStart);
            }
        }
    }

    if (!downloadUrlStr.empty()) {
        result.downloadUrl = std::wstring(downloadUrlStr.begin(), downloadUrlStr.end());
    } else {
        // Enlace de respaldo a la página de releases
        result.downloadUrl = GITHUB_REPO_URL + std::wstring(L"/releases");
    }

    result.success = true;
    result.updateAvailable = IsNewerVersion(latestVer, APP_VERSION);
    return result;
}

bool Updater::ApplyUpdate(const std::wstring& downloadUrl, std::wstring& outError) {
    if (downloadUrl.empty()) {
        outError = L"URL de descarga no disponible.";
        return false;
    }

    std::wstring currentExe = LuxUtils::GetExePath();
    std::wstring newExe = currentExe + L".new";
    std::wstring oldExe = currentExe + L".old";

    // 1. Descargar archivo
    HRESULT hr = URLDownloadToFileW(nullptr, downloadUrl.c_str(), newExe.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        outError = L"Error al descargar la nueva versión.";
        DeleteFileW(newExe.c_str());
        return false;
    }

    // 2. Renombrar ejecutable actual a .old (Windows permite renombrar archivos en ejecución)
    DeleteFileW(oldExe.c_str());
    if (!MoveFileW(currentExe.c_str(), oldExe.c_str())) {
        outError = L"No se pudo preparar el reemplazo del ejecutable actual.";
        DeleteFileW(newExe.c_str());
        return false;
    }

    // 3. Colocar el nuevo ejecutable en su lugar
    if (!MoveFileW(newExe.c_str(), currentExe.c_str())) {
        // Rollback
        MoveFileW(oldExe.c_str(), currentExe.c_str());
        outError = L"No se pudo instalar el nuevo archivo ejecutable.";
        return false;
    }

    // 4. Iniciar la nueva versión
    HINSTANCE hInst = ShellExecuteW(nullptr, L"open", currentExe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
        outError = L"Actualizado con éxito, pero ocurrió un error al reiniciar la aplicación.";
        return false;
    }

    // 5. Salir del proceso actual para ceder el control
    ExitProcess(0);
    return true;
}

void Updater::CleanOldFiles() {
    std::wstring currentExe = LuxUtils::GetExePath();
    std::wstring oldExe = currentExe + L".old";
    std::wstring newExe = currentExe + L".new";
    DeleteFileW(oldExe.c_str());
    DeleteFileW(newExe.c_str());
}
