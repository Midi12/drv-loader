#pragma once

#include "helpers.hpp"
#include "lazy_loader_light.hpp"

#include <filesystem>

#define NOMINMAX
#include <windows.h>

#include "ntstatus.hpp"

namespace drv_loader {

	typedef struct _UNICODE_STRING {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR  Buffer;
	} UNICODE_STRING, * PUNICODE_STRING;

	constexpr char prefix[] = "\\??\\";
	constexpr char registry_subkey[] = "System\\CurrentControlSet\\Services\\";
	constexpr char registry_prefix[] = "\\Registry\\Machine\\";

	typedef enum _loader_operation_t {
		none,
		load,
		unload,

		// number of entries in enum
		n_loader_operation
	} loader_operation_t;

	typedef struct _config_t {
		std::string display_name;
		std::string file_path;
		loader_operation_t operation;
	} config_t, *pconfig_t;

	static std::uint32_t load_driver(const config_t& config) {
		if (config.operation != loader_operation_t::load) {
			return ERROR_INVALID_OPERATION;
		}

		std::filesystem::path path = config.file_path;
		std::string ntpath = std::string(prefix) + std::filesystem::absolute(path).string();

		std::string reg_path = std::string(registry_subkey) + config.display_name;

		HKEY key_handle = nullptr;
		LSTATUS status = ::RegCreateKeyExA(HKEY_LOCAL_MACHINE, reg_path.c_str(), NULL, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_QUERY_VALUE, nullptr, &key_handle, nullptr);
		if (status != ERROR_SUCCESS) {
			return status;
		}

		status = ::RegSetValueExA(key_handle, "ImagePath", NULL, REG_EXPAND_SZ, reinterpret_cast<const std::uint8_t*>(ntpath.c_str()), static_cast<std::uint32_t>(ntpath.size()));
		if (status != ERROR_SUCCESS) {
			return status;
		}

		constexpr std::uint32_t type = 1;
		status = ::RegSetValueExA(key_handle, "Type", NULL, REG_DWORD, reinterpret_cast<const std::uint8_t*>(&type), sizeof(type));
		if (status != ERROR_SUCCESS) {
			return status;
		}

		constexpr std::uint32_t start = 3;
		status = ::RegSetValueExA(key_handle, "Start", NULL, REG_DWORD, reinterpret_cast<const std::uint8_t*>(&start), sizeof(start));
		if (status != ERROR_SUCCESS) {
			return status;
		}

		constexpr std::uint32_t error_control = 1;
		status = ::RegSetValueExA(key_handle, "ErrorControl", NULL, REG_DWORD, reinterpret_cast<const std::uint8_t*>(&error_control), sizeof(error_control));
		if (status != ERROR_SUCCESS) {
			return status;
		}

		status = ::RegSetValueExA(key_handle, "DisplayName", NULL, REG_SZ, reinterpret_cast<const std::uint8_t*>(config.display_name.c_str()), static_cast<std::uint32_t>(config.display_name.size()));
		if (status != ERROR_SUCCESS) {
			return status;
		}

		if (key_handle != nullptr) {
			::RegCloseKey(key_handle);
		}

		UNICODE_STRING nt_reg_path = {};

		std::wstring reg_path_copy = helpers::to_unicode(std::string(registry_prefix) + reg_path);
		LAZYCALL(void, "ntdll.dll!RtlInitUnicodeString", &nt_reg_path, reg_path_copy.c_str());
		
		NTSTATUS nt_status = LAZYCALL(NTSTATUS, "ntdll.dll!NtLoadDriver", &nt_reg_path);

		if (nt_status == STATUS_IMAGE_ALREADY_LOADED) {
			LAZYCALL(NTSTATUS, "ntdll.dll!NtUnloadDriver", &nt_reg_path);
			LAZYCALL(NTSTATUS, "ntdll.dll!NtYieldExecution");
			nt_status = LAZYCALL(NTSTATUS, "ntdll.dll!NtLoadDriver", &nt_reg_path);
		}

		if (nt_status == STATUS_SUCCESS) {
			return ERROR_SUCCESS;
		}

		ULONG converted_status = LAZYCALL(ULONG, "ntdll!RtlNtStatusToDosError", nt_status);
		if (converted_status == ERROR_MR_MID_NOT_FOUND) {
			return nt_status;
		}

		return static_cast<std::uint32_t>(converted_status);
	}

	static std::uint32_t unload_driver(const config_t& config) {
		if (config.operation != loader_operation_t::unload) {
			return ERROR_INVALID_OPERATION;
		}

		std::string reg_path = std::string(registry_subkey) + config.display_name;

		UNICODE_STRING nt_reg_path = {};

		std::wstring reg_path_copy = helpers::to_unicode(std::string(registry_prefix) + reg_path);
		LAZYCALL(void, "ntdll.dll!RtlInitUnicodeString", &nt_reg_path, reg_path_copy.c_str());

		NTSTATUS nt_status = LAZYCALL(NTSTATUS, "ntdll.dll!NtUnloadDriver", &nt_reg_path);

		if (nt_status != STATUS_SUCCESS) {
			ULONG converted_status = LAZYCALL(ULONG, "ntdll!RtlNtStatusToDosError", nt_status);
			if (converted_status == ERROR_MR_MID_NOT_FOUND) {
				return nt_status;
			}

			return static_cast<std::uint32_t>(converted_status);
		}

		return static_cast<std::uint32_t>(::RegDeleteTreeA(HKEY_LOCAL_MACHINE, reg_path.c_str()));
	}

	static std::uint32_t load_unload(const config_t& config) {
		std::uint32_t ret = ERROR_INVALID_OPERATION;

		switch (config.operation) {
			case loader_operation_t::load:
				ret = load_driver(config);
				break;
			case loader_operation_t::unload:
				ret = unload_driver(config);
				break;
			default:
				break;
		}

		return ret;
	}
}