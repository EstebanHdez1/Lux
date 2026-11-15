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

    // Buscar en los assets de GitHub Releases:
    // Prioridad 1: Versión portable en archivo .zip (ej. Lux-v1.0.2-Windows-Portable.zip)
    std::string downloadUrlStr;
    size_t zipPos = json.find(".zip");
    if (zipPos != std::string::npos) {
        size_t urlTagPos = json.rfind("\"browser_download_url\"", zipPos);
        if (urlTagPos != std::string::npos) {
            size_t valStart = json.find("https://", urlTagPos);
            if (valStart != std::string::npos && valStart < zipPos) {
                downloadUrlStr = json.substr(valStart, (zipPos + 4) - valStart);
            }
        }
    }

    // Prioridad 2: Fallback a archivo binario .exe directo
    if (downloadUrlStr.empty()) {
        size_t exePos = json.find(".exe");
        if (exePos != std::string::npos) {
            size_t urlTagPos = json.rfind("\"browser_download_url\"", exePos);
            if (urlTagPos != std::string::npos) {
                size_t valStart = json.find("https://", urlTagPos);
                if (valStart != std::string::npos && valStart < exePos) {
                    downloadUrlStr = json.substr(valStart, (exePos + 4) - valStart);
                }
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

static std::wstring FindFileRecursive(const std::wstring& dir, const std::wstring& targetFilename) {
    std::wstring searchPath = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return L"";

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring found = FindFileRecursive(fullPath, targetFilename);
            if (!found.empty()) {
                FindClose(hFind);
                return found;
            }
        } else {
            if (_wcsicmp(fd.cFileName, targetFilename.c_str()) == 0) {
                FindClose(hFind);
                return fullPath;
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return L"";
}

bool Updater::ApplyUpdate(const std::wstring& downloadUrl, std::wstring& outError) {
    if (downloadUrl.empty()) {
        outError = L"URL de descarga no disponible.";
        return false;
    }

    std::wstring currentExe = LuxUtils::GetExePath();
    std::wstring newExe = currentExe + L".new";
    std::wstring oldExe = currentExe + L".old";

    wchar_t tempPathBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPathBuf);
    std::wstring tempDir = tempPathBuf;

    bool isZip = (downloadUrl.length() >= 4 && _wcsicmp(downloadUrl.c_str() + downloadUrl.length() - 4, L".zip") == 0);

    if (isZip) {
        std::wstring tempZip = tempDir + L"Lux_Portable_Update.zip";
        std::wstring tempExtract = tempDir + L"Lux_Portable_Extract";

        // 1. Descargar paquete ZIP portable
        DeleteFileW(tempZip.c_str());
        HRESULT hr = URLDownloadToFileW(nullptr, downloadUrl.c_str(), tempZip.c_str(), 0, nullptr);
        if (FAILED(hr)) {
            outError = L"Error al descargar la versión portable.";
            DeleteFileW(tempZip.c_str());
            return false;
        }

        // 2. Crear directorio de extracción y descomprimir con tar.exe nativo de Windows
        CreateDirectoryW(tempExtract.c_str(), nullptr);
        std::wstring cmd = L"tar.exe -xf \"" + tempZip + L"\" -C \"" + tempExtract + L"\"";
        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        BOOL procOk = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (procOk) {
            WaitForSingleObject(pi.hProcess, 15000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        // 3. Localizar Lux.exe dentro de los archivos extraídos
        std::wstring extractedExe = FindFileRecursive(tempExtract, L"Lux.exe");
        if (extractedExe.empty()) {
            outError = L"No se encontró el ejecutable en el paquete portable.";
            DeleteFileW(tempZip.c_str());
            return false;
        }

        // Copiar a .new
        DeleteFileW(newExe.c_str());
        if (!CopyFileW(extractedExe.c_str(), newExe.c_str(), FALSE)) {
            outError = L"No se pudo preparar el binario extraído.";
            DeleteFileW(tempZip.c_str());
            return false;
        }

        // Limpiar archivo temporal zip
        DeleteFileW(tempZip.c_str());
    } else {
        // Descarga directa tradicional de ejecutable
        HRESULT hr = URLDownloadToFileW(nullptr, downloadUrl.c_str(), newExe.c_str(), 0, nullptr);
        if (FAILED(hr)) {
            outError = L"Error al descargar la nueva versión.";
            DeleteFileW(newExe.c_str());
            return false;
        }
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
