#pragma once

#include <locale>
#include <codecvt>
#include <string>
#include <cstdint>
#include <charconv>
#include <array>

#define NOMINMAX
#include <windows.h>

namespace helpers {

	static std::string to_ansi(const std::wstring& wstr) {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes(wstr);
	}

	static std::wstring to_unicode(const std::string& str) {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(str);
	}

	template <typename T>
	static std::string to_hex_string(T number) {
		static_assert(std::is_integral<T>::value, "helpers::to_hex_string is only available for integral types");

		constexpr std::size_t buffer_size = std::is_signed<T>::value ? (sizeof(T) * 2) + 1 : sizeof(T) * 2;
		std::array<char, buffer_size> arr;

		auto res = std::to_chars(arr.data(), arr.data() + arr.size(), number, 16);

		return std::string(arr.data(), std::distance(arr.data(), res.ptr));
	}

	template <typename T>
	static T from_hex_string(const std::string& string) {
		static_assert(std::is_integral<T>::value, "helpers::from_hex_string is only available for integral types");

		T result;
		std::from_chars(string.data(), string.data() + string.size(), result, 16);

		return result;
	}

	template<class T>
	class scoped_ptr {
		private:
			T* _ptr = nullptr;
		public:
			typedef T element_type;

			explicit scoped_ptr(T* ptr = nullptr) : _ptr(ptr) {}
			scoped_ptr(const scoped_ptr&) = delete; // non copyable
			~scoped_ptr(void) { if (_ptr != nullptr) { delete _ptr; } }

			void reset(T* ptr = nullptr) { _ptr = ptr; };

			T& operator*(void) const { return *_ptr; }
			T* operator->(void) const { return _ptr; }
			T* get(void) const { return _ptr; }
	};

	template<>
	class scoped_ptr<void> {
		private:
			void* _ptr = nullptr;
		public:
			typedef void element_type;

			explicit scoped_ptr(void* ptr = nullptr) : _ptr(ptr) {}
			scoped_ptr(const scoped_ptr&) = delete; // non copyable
			~scoped_ptr(void) { if (_ptr != nullptr) { delete _ptr; } }

			void reset(void* ptr = nullptr) { _ptr = ptr; };

			void* operator->(void) const { return _ptr; }
			void* get(void) const { return _ptr; }
	};

	template<>
	class scoped_ptr<void*> {
	private:
		void* _ptr = nullptr;
	public:
		typedef void element_type;

		explicit scoped_ptr(void* ptr = nullptr) : _ptr(ptr) {}
		scoped_ptr(const scoped_ptr&) = delete; // non copyable
		~scoped_ptr(void) { if (_ptr != nullptr) { delete _ptr; } }

		void reset(void* ptr = nullptr) {
			if (_ptr != nullptr) { delete _ptr; }
			_ptr = ptr;
		};

		void* operator->(void) const { return _ptr; }
		void* get(void) const { return _ptr; }
	};

	class scoped_handle {
		private:
			HANDLE _handle = nullptr;

			void safe_close_handle(void) {
				if (_handle != INVALID_HANDLE_VALUE) {
					::CloseHandle(_handle);
					_handle = INVALID_HANDLE_VALUE;
				}
			}
		public:
			typedef HANDLE element_type;

			explicit scoped_handle(HANDLE handle = INVALID_HANDLE_VALUE) : _handle(handle) {}
			scoped_handle(const scoped_handle&) = delete; // non copyable
			~scoped_handle(void) { safe_close_handle(); }

			void reset(HANDLE handle = INVALID_HANDLE_VALUE) {
				safe_close_handle();
				_handle = handle;
			}

			HANDLE get(void) const { return _handle; }
			HANDLE *get_ptr(void) { return &_handle; }
	};

	bool add_privilege(const std::string& name) {
		bool ret = false;
		scoped_handle token_handle;
		TOKEN_PRIVILEGES privileges = {};

		BOOL open_status = ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, token_handle.get_ptr());

		if (open_status) {
			BOOL lookup_status = ::LookupPrivilegeValueA(nullptr, name.c_str(), &privileges.Privileges[0].Luid);

			if (lookup_status) {
				privileges.PrivilegeCount = 1;
				privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

				ret = static_cast<bool>(::AdjustTokenPrivileges(token_handle.get(), false, &privileges, 0, nullptr, nullptr));
			}
		}

		return ret;
	}
}