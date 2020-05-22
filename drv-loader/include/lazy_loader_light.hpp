#pragma once

#include "functor.hpp"

#include <string>
#include <regex>

#define NOMINMAX
#include <windows.h>

// compact and light lazy loader version (no exception, no litterals, basic modules and imports management)

namespace lazy_loader_light {

	static bool is_import_str(const std::string& str) {
		return !str.empty() && std::all_of(str.begin(), str.end(), [](const auto& c) -> bool { return isalnum(c) || c == '!' || c == '_' || c == '.' || c == '/'; /*todo : better check*/ });
	}

	static std::pair<std::string, std::string> split_import_string(const std::string& str) {
		//assert(is_import_str(str));

		std::regex reg("!");
		std::sregex_token_iterator iter(str.begin(), str.end(), reg, -1);
		std::sregex_token_iterator end;

		std::vector<std::string> splitted(iter, end);

		assert(splitted.size() >= 1 || splitted.size() <= 2);

		std::pair<std::string, std::string> ret = std::make_pair(splitted.at(0), "");

		if (splitted.size() > 1) {
			ret = std::make_pair(splitted.at(0), splitted.at(1));
		}

		return ret;
	}

#if defined(_WIN64)
	struct WindowsLoader {
		static std::uintptr_t load_module(const std::string& module_name) {
			return reinterpret_cast<std::uintptr_t>(::LoadLibraryA(module_name.c_str()));
		}

		static std::uintptr_t get_symbol(const std::uintptr_t module_handle, const std::string& symbol_name) {
			return reinterpret_cast<std::uintptr_t>(::GetProcAddress(reinterpret_cast<HMODULE>(module_handle), symbol_name.c_str()));
		}

		static std::uint32_t free_module(const std::uintptr_t module_handle) {
			return ::FreeLibrary(reinterpret_cast<HMODULE>(module_handle));
		}
	};
#elif defined(__linux__) or defined(__APPLE__)
	struct UnixLoader {
		static std::uintptr_t load_module(const std::string& module_name) {
			return reinterpret_cast<std::uintptr_t>(dlopen(module_name.c_str(), RTLD_NOW));
		}

		static std::uintptr_t get_symbol(const std::uintptr_t module_handle, const std::string& symbol_name) {
			return reinterpret_cast<std::uintptr_t>(dlsym(reinterpret_cast<void*>(module_handle), symbol_name.c_str()));
		}

		static std::uint32_t free_module(const std::uintptr_t module_handle) {
			return dlclose(reinterpret_cast<void*>(module_handle));
		}
	};
#endif

	class lazyimport {
		public:
			lazyimport() = default;

			lazyimport(const std::string& name, std::uintptr_t ptr)
				: _name(name), _ptr(ptr)
			{
				static std::hash<std::string> hashfn;
				_hash = hashfn(name);
			}

			lazyimport(const lazyimport&) = default;

			lazyimport& operator= (const lazyimport&) = default;

			lazyimport(lazyimport&&) noexcept = default;

			~lazyimport() = default;

			template <typename ReturnType, typename ...Args>
			ReturnType operator()(Args&&... args) const {
				Functor<ReturnType(*)(Args ...)> functor(_ptr);
				return functor(std::forward<Args>(args)...);
			}

			template <typename ReturnType, typename ...Args>
			ReturnType call(Args&&... args) const {
				Functor<ReturnType(*)(Args ...)> functor(_ptr);
				return functor(std::forward<Args>(args)...);
			}

			operator bool() const {
				return _ptr != 0;
			}

			std::string name() const {
				return _name;
			}

			std::uintptr_t ptr() const {
				return _ptr;
			}

			std::size_t hash() const {
				return _hash;
			}
		private:
			std::string _name;
			std::size_t _hash;
			std::uintptr_t _ptr = 0;
	};

	template <typename LoaderTraits>
	class basic_lazyimportcollection {
		public:
			basic_lazyimportcollection() = default;

			basic_lazyimportcollection(const basic_lazyimportcollection&) = default;

			basic_lazyimportcollection& operator= (const basic_lazyimportcollection&) = default;

			basic_lazyimportcollection(basic_lazyimportcollection&&) noexcept = default;

			~basic_lazyimportcollection() = default;

			void find_or_load(const std::uintptr_t& handle, const std::string& function_name, lazyimport& elem) {
				auto it = std::find_if(_collection.begin(), _collection.end(), [&function_name](const lazyimport& import) -> bool {
					static std::hash<std::string> hash_fn;
					return hash_fn(function_name) == import.hash();
				});

				if (it != _collection.end()) {
					elem = *it;
				} else {
					std::uintptr_t ptr = LoaderTraits::get_symbol(handle, function_name.c_str());

					if (ptr == 0) {
						std::string err = "cannot load function " + function_name;

#if defined(__linux__) or defined(__APPLE__)
						err += " (" + std::string(dlerror()) + ")";
#endif

						return;
					}

					elem = _collection.emplace_back(function_name, ptr);
				}
			}

			std::size_t size() const {
				return _collection.size();
			}

		private:
			std::vector<lazyimport> _collection;
	};

	template <typename LoaderTraits>
	class basic_lazymodule {
		public:
			basic_lazymodule() = default;

			basic_lazymodule(const std::string& name, std::uintptr_t hmod)
				: _name(name), _handle(hmod)
			{
				static std::hash<std::string> hashfn;
				_hash = hashfn(name);
			}

			basic_lazymodule(const basic_lazymodule&) = default;

			basic_lazymodule<LoaderTraits>& operator= (const basic_lazymodule& mod) = default;

			basic_lazymodule(basic_lazymodule&&) noexcept = default;

			~basic_lazymodule() = default;

			std::uint32_t unload() {
				return LoaderTraits::free_module(_handle);
			}

			std::string name() const {
				return _name;
			}

			std::uintptr_t handle() const {
				return _handle;
			}

			std::size_t hash() const {
				return _hash;
			}

			basic_lazyimportcollection<LoaderTraits> imports() const {
				return _imports;
			}

			void add(const std::string& function_name, lazyimport& import) {
				_imports.find_or_load(_handle, function_name, import);
			}
		private:
			std::string _name;
			std::uintptr_t _handle = 0;
			std::size_t _hash;
			basic_lazyimportcollection<LoaderTraits> _imports;
	};

	template <typename LoaderTraits>
	class basic_lazymodulecollection {
		private:
			basic_lazymodulecollection() = default;

			~basic_lazymodulecollection() {
				for (auto& mod : _collection) {
					mod.unload();
				}
			};

			basic_lazymodulecollection(const basic_lazymodulecollection&) = delete;
			basic_lazymodulecollection& operator= (const basic_lazymodulecollection&) = delete;
			basic_lazymodulecollection(basic_lazymodulecollection&&) noexcept = default;

		public:
			static basic_lazymodulecollection& instance() {
				static basic_lazymodulecollection instance;
				return instance;
			}

			std::size_t size() const {
				return _collection.size();
			}

			lazyimport register_import(const std::string& module_name, const std::string& function_name) {
				lazyimport import;

				basic_lazymodule<LoaderTraits> module;

				if (!module_name.empty()) {
					find_or_load(module_name, module);

					if (!function_name.empty()) {
						module.add(function_name, import);
					}
				}

				return import;
			}

			lazyimport register_import(const std::string& path) {
				auto import_data = split_import_string(path);
				return register_import(import_data.first, import_data.second);
			}

			void find_or_load(const std::string& name, basic_lazymodule<LoaderTraits>& elem) {
				auto it = std::find_if(_collection.begin(), _collection.end(), [&name](const basic_lazymodule<LoaderTraits>& module) -> bool {
					static std::hash<std::string> hash_fn;
					return hash_fn(name) == module.hash();
				});

				if (it != _collection.end()) {
					elem = *it;
				}
				else {
					std::uintptr_t hmod = LoaderTraits::load_module(name.c_str());

					if (hmod == 0) {
						std::string err = "cannot load module " + name;

#if defined(__linux__) or defined(__APPLE__)
						err += " (" + std::string(dlerror()) + ")";
#endif
						return;
					}

					elem = _collection.emplace_back(basic_lazymodule<LoaderTraits>(name, hmod));
				}
			}

			void unload(const std::string& name) {
				auto it = std::find_if(_collection.begin(), _collection.end(), [&name](const basic_lazymodule<LoaderTraits>& module) -> bool {
					static std::hash<std::string> hash_fn;
					return hash_fn(name) == module.hash();
				});

				if (it != _collection.end()) {
					basic_lazymodule<LoaderTraits> mod = *it;
					mod.unload();
					_collection.erase(it);
				}
			}

		private:
			using modulecollection = std::vector<basic_lazymodule<LoaderTraits>>;
			modulecollection _collection;
	};

#if defined(_WIN64)
	using lazymodule = basic_lazymodule<WindowsLoader>;
	using lazymodulecollection = basic_lazymodulecollection<WindowsLoader>;
#elif defined(__linux__) or defined(__APPLE__)
	using lazymodule = basic_lazymodule<UnixLoader>;
	using lazymodulecollection = basic_lazymodulecollection<UnixLoader>;
#endif

#define LAZYCALL(ReturnType, path, ...) \
	::lazy_loader_light::lazymodulecollection::instance().register_import(path).call<ReturnType>(__VA_ARGS__)

#define LAZYLOAD(path) \
	::lazy_loader_light::lazymodulecollection::instance().register_import(path)

#define LAZYUNLOAD(path) \
	::lazy_loader_light::lazymodulecollection::instance().unload(path)
}

