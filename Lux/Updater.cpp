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
    // Prioridad 1: Binario directo Lux.exe (instalación rápida, atómica y sin dependencias)
    // Prioridad 2: Paquete portable en archivo .zip
    std::string bestExeUrl;
    std::string bestZipUrl;

    size_t assetsPos = json.find("\"assets\":");
    if (assetsPos != std::string::npos) {
        size_t searchPos = assetsPos;
        const std::string key = "\"browser_download_url\"";
        while (true) {
            size_t tagPos = json.find(key, searchPos);
            if (tagPos == std::string::npos) break;

            size_t colonPos = json.find(':', tagPos + key.length());
            if (colonPos == std::string::npos) break;

            size_t quoteStart = json.find('"', colonPos);
            if (quoteStart == std::string::npos) break;

            size_t quoteEnd = json.find('"', quoteStart + 1);
            if (quoteEnd == std::string::npos) break;

            std::string url = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            searchPos = quoteEnd + 1;

            if (url.rfind("https://", 0) == 0) {
                std::string urlLower = url;
                for (char& c : urlLower) c = static_cast<char>(tolower(c));

                if (urlLower.length() >= 4 && urlLower.compare(urlLower.length() - 4, 4, ".exe") == 0) {
                    if (bestExeUrl.empty()) bestExeUrl = url;
                } else if (urlLower.length() >= 4 && urlLower.compare(urlLower.length() - 4, 4, ".zip") == 0) {
                    if (bestZipUrl.empty()) bestZipUrl = url;
                }
            }
        }
    }

    std::string selectedUrl = !bestExeUrl.empty() ? bestExeUrl : bestZipUrl;

    if (!selectedUrl.empty()) {
        result.downloadUrl = std::wstring(selectedUrl.begin(), selectedUrl.end());
    } else {
        // Enlace de respaldo a la página oficial de releases
        result.downloadUrl = GITHUB_REPO_URL + std::wstring(L"/releases");
    }

    result.success = true;
    result.updateAvailable = IsNewerVersion(latestVer, APP_VERSION);
    return result;
}

static bool DownloadFileDirect(const std::wstring& url, const std::wstring& destPath, std::wstring& outError) {
    HINTERNET hInternet = InternetOpenW(L"Lux-Updater", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        outError = L"No se pudo inicializar la conexión a Internet.";
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE;
    const wchar_t* headers = L"User-Agent: Lux-App\r\nAccept: */*\r\n";

    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), headers, -1, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        outError = L"No se pudo abrir la dirección de descarga del servidor.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, nullptr)) {
        if (statusCode != 200) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            wchar_t codeBuf[64];
            swprintf_s(codeBuf, L"El servidor respondió con código HTTP %u.", statusCode);
            outError = codeBuf;
            return false;
        }
    }

    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        outError = L"No se pudo crear el archivo temporal de destino en disco.";
        return false;
    }

    BYTE buffer[16384];
    DWORD bytesRead = 0;
    DWORD bytesWritten = 0;
    bool writeFailed = false;
    DWORD totalDownloaded = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead) {
            writeFailed = true;
            break;
        }
        totalDownloaded += bytesRead;
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (writeFailed || totalDownloaded < 1024) {
        DeleteFileW(destPath.c_str());
        outError = writeFailed ? L"Error al escribir datos en disco." : L"El archivo descargado está incompleto o dañado.";
        return false;
    }

    return true;
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

    bool isZip = (downloadUrl.length() >= 4 && _wcsicmp(downloadUrl.c_str() + downloadUrl.length() - 4, L".zip") == 0);
    bool isExe = (downloadUrl.length() >= 4 && _wcsicmp(downloadUrl.c_str() + downloadUrl.length() - 4, L".exe") == 0);

    if (!isZip && !isExe) {
        ShellExecuteW(nullptr, L"open", downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        outError = L"Se abrió la página oficial de descargas en tu navegador.";
        return false;
    }

    std::wstring currentExe = LuxUtils::GetExePath();
    std::wstring newExe = currentExe + L".new";
    std::wstring oldExe = currentExe + L".old";

    wchar_t tempPathBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPathBuf);
    std::wstring tempDir = tempPathBuf;

    if (isZip) {
        std::wstring tempZip = tempDir + L"Lux_Portable_Update.zip";
        std::wstring tempExtract = tempDir + L"Lux_Portable_Extract";

        // 1. Descargar paquete ZIP portable con WinINet nativo
        DeleteFileW(tempZip.c_str());
        if (!DownloadFileDirect(downloadUrl, tempZip, outError)) {
            return false;
        }

        // 2. Limpiar directorio previo y descomprimir con tar.exe nativo
        std::wstring cleanCmd = L"cmd.exe /c if exist \"" + tempExtract + L"\" rd /s /q \"" + tempExtract + L"\"";
        _wsystem(cleanCmd.c_str());
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
        // Descarga directa limpia de ejecutable con WinINet
        DeleteFileW(newExe.c_str());
        if (!DownloadFileDirect(downloadUrl, newExe, outError)) {
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

    // 4. Iniciar la nueva versión pasando --restart para que espere la liberación del mutex
    HINSTANCE hInst = ShellExecuteW(nullptr, L"open", currentExe.c_str(), L"--restart", nullptr, SW_SHOWNORMAL);
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

    DWORD attrOld = GetFileAttributesW(oldExe.c_str());
    if (attrOld != INVALID_FILE_ATTRIBUTES && !(attrOld & FILE_ATTRIBUTE_DIRECTORY)) {
        DeleteFileW(oldExe.c_str());
    }
    DWORD attrNew = GetFileAttributesW(newExe.c_str());
    if (attrNew != INVALID_FILE_ATTRIBUTES && !(attrNew & FILE_ATTRIBUTE_DIRECTORY)) {
        DeleteFileW(newExe.c_str());
    }
}
