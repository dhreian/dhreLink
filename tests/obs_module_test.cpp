#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using ObsStartup = bool (*)(const char*, const char*, void*);
using ObsShutdown = void (*)();
using ObsOpenModule = int (*)(void**, const char*, const char*);
using ObsInitModule = bool (*)(void*);
using ObsEnumInputTypes = bool (*)(std::size_t, const char**);
using ObsSourceGetDisplayName = const char* (*)(const char*);
using ObsSourceCreate = void* (*)(const char*, const char*, void*, void*);
using ObsSourceRelease = void (*)(void*);
using ObsSourceProperties = void* (*)(const void*);
using ObsPropertiesGet = void* (*)(void*, const char*);
using ObsPropertiesDestroy = void (*)(void*);
using ObsSourceGetIcon = const char* (*)(const char*);
using BFree = void (*)(void*);

std::string utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const auto size = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
    nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string result(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr) != size) {
    return {};
  }
  return result;
}

std::wstring utf16(std::string_view value) {
  if (value.empty()) return {};
  const auto size = MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
    nullptr, 0);
  if (size <= 0) return {};
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size) != size) {
    return {};
  }
  return result;
}

template <typename Function>
Function loadFunction(HMODULE module, const char* name) {
  return reinterpret_cast<Function>(GetProcAddress(module, name));
}

} // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 4) {
    std::cerr << "Expected paths to obs.dll, the dhreLink module, and its data.\n";
    return 1;
  }

  const std::filesystem::path obsPath(argv[1]);
  const std::filesystem::path pluginPath(argv[2]);
  const std::filesystem::path dataPath(argv[3]);
  if (
    !std::filesystem::is_regular_file(obsPath) ||
    !std::filesystem::is_regular_file(pluginPath) ||
    !std::filesystem::is_directory(dataPath)) {
    std::cerr << "A required module file does not exist.\n";
    return 2;
  }

  if (!SetDllDirectoryW(obsPath.parent_path().c_str())) {
    std::cerr << "Could not configure the OBS runtime directory.\n";
    return 3;
  }
  const auto obs = LoadLibraryW(obsPath.c_str());
  if (obs == nullptr) {
    std::cerr << "Could not load obs.dll (Windows error " << GetLastError() << ").\n";
    SetDllDirectoryW(nullptr);
    return 3;
  }

  const auto startup = loadFunction<ObsStartup>(obs, "obs_startup");
  const auto shutdown = loadFunction<ObsShutdown>(obs, "obs_shutdown");
  const auto openModule = loadFunction<ObsOpenModule>(obs, "obs_open_module");
  const auto initModule = loadFunction<ObsInitModule>(obs, "obs_init_module");
  const auto enumInputTypes = loadFunction<ObsEnumInputTypes>(obs, "obs_enum_input_types");
  const auto sourceGetDisplayName =
    loadFunction<ObsSourceGetDisplayName>(obs, "obs_source_get_display_name");
  const auto sourceCreate = loadFunction<ObsSourceCreate>(obs, "obs_source_create");
  const auto sourceRelease = loadFunction<ObsSourceRelease>(obs, "obs_source_release");
  const auto sourceProperties =
    loadFunction<ObsSourceProperties>(obs, "obs_source_properties");
  const auto propertiesGet = loadFunction<ObsPropertiesGet>(obs, "obs_properties_get");
  const auto propertiesDestroy =
    loadFunction<ObsPropertiesDestroy>(obs, "obs_properties_destroy");
  const auto getDarkIcon = loadFunction<ObsSourceGetIcon>(obs, "obs_source_get_dark_icon");
  const auto getLightIcon = loadFunction<ObsSourceGetIcon>(obs, "obs_source_get_light_icon");
  const auto freeMemory = loadFunction<BFree>(obs, "bfree");
  if (
    startup == nullptr || shutdown == nullptr || openModule == nullptr ||
    initModule == nullptr || enumInputTypes == nullptr || sourceGetDisplayName == nullptr ||
    sourceCreate == nullptr ||
    sourceRelease == nullptr || sourceProperties == nullptr || propertiesGet == nullptr ||
    propertiesDestroy == nullptr || getDarkIcon == nullptr || getLightIcon == nullptr ||
    freeMemory == nullptr) {
    std::cerr << "The OBS runtime is missing a required API.\n";
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 4;
  }

  if (!startup("en-US", nullptr, nullptr)) {
    std::cerr << "obs_startup failed.\n";
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 5;
  }

  const auto pluginUtf8 = utf8(pluginPath.wstring());
  const auto dataUtf8 = utf8(dataPath.wstring());
  void* module = nullptr;
  const auto openResult = openModule(&module, pluginUtf8.c_str(), dataUtf8.c_str());
  if (openResult != 0 || module == nullptr) {
    std::cerr << "OBS rejected the dhreLink module (result " << openResult << ").\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 6;
  }
  if (!initModule(module)) {
    std::cerr << "dhreLink obs_module_load failed.\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 7;
  }

  bool found = false;
  for (std::size_t index = 0;; ++index) {
    const char* id = nullptr;
    if (!enumInputTypes(index, &id)) break;
    if (id != nullptr && std::string_view(id) == "dhrelink_audio_source") {
      found = true;
      break;
    }
  }

  if (!found) {
    std::cerr << "The dhreLink input source was not registered.\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 8;
  }

  const auto displayName = sourceGetDisplayName("dhrelink_audio_source");
  if (displayName == nullptr || std::string_view(displayName) != "dhreLink Receiver") {
    std::cerr << "The dhreLink source has an unexpected display name.\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 9;
  }

  const auto darkIcon = getDarkIcon("dhrelink_audio_source");
  const auto lightIcon = getLightIcon("dhrelink_audio_source");
  const auto iconsExist =
    darkIcon != nullptr && lightIcon != nullptr &&
    std::filesystem::is_regular_file(std::filesystem::path(utf16(darkIcon))) &&
    std::filesystem::is_regular_file(std::filesystem::path(utf16(lightIcon)));
  freeMemory(const_cast<char*>(darkIcon));
  freeMemory(const_cast<char*>(lightIcon));
  if (!iconsExist) {
    std::cerr << "The dhreLink custom source icons are unavailable.\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 10;
  }

  const auto source = sourceCreate(
    "dhrelink_audio_source", "dhreLink module test", nullptr, nullptr);
  if (source == nullptr) {
    std::cerr << "OBS could not create the dhreLink input source.\n";
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    return 11;
  }

  auto* properties = sourceProperties(source);
  const auto propertiesAreComplete =
    properties != nullptr &&
    propertiesGet(properties, "dhrelink_status") != nullptr &&
    propertiesGet(properties, "dhrelink_route") != nullptr &&
    propertiesGet(properties, "dhrelink_format") != nullptr &&
    propertiesGet(properties, "dhrelink_help") != nullptr &&
    propertiesGet(properties, "dhrelink_credit") != nullptr;
  if (properties != nullptr) propertiesDestroy(properties);
  if (!propertiesAreComplete) {
    sourceRelease(source);
    shutdown();
    FreeLibrary(obs);
    SetDllDirectoryW(nullptr);
    std::cerr << "The dhreLink OBS properties panel is incomplete.\n";
    return 12;
  }
  sourceRelease(source);

  shutdown();
  FreeLibrary(obs);
  SetDllDirectoryW(nullptr);
  std::cout << "dhreLink OBS module loaded, registered, and created successfully.\n";
  return 0;
}
