#pragma once

#include <cstdint>

template<typename ...ArgumentTypes>
struct args_pack_t { };

template <typename FunctionType>
class Functor;

template <typename ResultType, typename ...ArgumentTypes>
class Functor<ResultType(*)(ArgumentTypes ...)> {
    public:
        typedef ResultType(*type)(ArgumentTypes ...);
        typedef ResultType result_type;
        typedef args_pack_t<ArgumentTypes ...> args_type;

        Functor(std::uintptr_t ptr) : _ptr(ptr) {}

        std::uintptr_t get(void) const {
            return _ptr;
        }

        ResultType operator() (ArgumentTypes&& ...args) const {
            return reinterpret_cast<ResultType(*)(ArgumentTypes ...)>(_ptr)(std::forward<ArgumentTypes>(args)...);
        }

    private:
        std::uintptr_t _ptr;
};