#pragma once
#include "beatsaber-hook/shared/hooking.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include <vector>

namespace MultiplayerCore {
    class Hooking {
        inline static std::vector<void (*)()> installFuncs;
    public:
        static void AddInstallFunc(void (*func)()) { installFuncs.push_back(func); }
        static void InstallHooks() { for (auto func : installFuncs) func(); }
    };
}

#define AUTO_INSTALL_PATCH(name_) \
    struct Auto_Patch_##name_ { \
        Auto_Patch_##name_() { ::MultiplayerCore::Hooking::AddInstallFunc(Patch_##name_); } \
    }; \
    static Auto_Patch_##name_ Auto_Patch_Instance_##name_

#define HOOK_AUTO_INSTALL_IMPL(name_, install_) \
    struct Auto_Hook_##name_ { \
        static void Install() { \
            static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID "_Install_" #name_); \
            install_(logger, name_); \
        } \
        Auto_Hook_##name_() { ::MultiplayerCore::Hooking::AddInstallFunc(Install); } \
    }; \
    static Auto_Hook_##name_ Auto_Hook_Instance_##name_

#define MAKE_AUTO_HOOK_IMPL(name_, method, retval, install_, ...) \
    struct hook_##name_ { \
        static constexpr auto cast_test = []<typename T>() { return requires { static_cast<T>(method); }; }; \
        using func_t = retval (*)(__VA_ARGS__); \
        using cast_t = ::i2c::detail::method_check<cast_test, func_t>::type; \
        static_assert(cast_test.operator()<cast_t>(), "Hook method signature does not match!"); \
        static_assert(::i2c::detail::match_hookable<static_cast<cast_t>(method)>, "Method cannot be hooked!"); \
        __INTERNAL_HOOK_STRUCT(name_, ::i2c::metadata_getter<static_cast<cast_t>(method)>::method_info(), retval, __VA_ARGS__) \
    }; \
    HOOK_AUTO_INSTALL_IMPL(name_, install_); \
    retval hook_##name_::hook_m_##name_(__VA_ARGS__)

#define MAKE_AUTO_HOOK_MATCH(name_, method, retval, ...) \
    MAKE_AUTO_HOOK_IMPL(name_, method, retval, INSTALL_HOOK, __VA_ARGS__)
#define MAKE_AUTO_HOOK_ORIG_MATCH(name_, method, retval, ...) \
    MAKE_AUTO_HOOK_IMPL(name_, method, retval, INSTALL_HOOK_ORIG, __VA_ARGS__)
#define MAKE_AUTO_HOOK_FIND_VERBOSE(name_, infoGet, retval, ...) \
    struct hook_##name_ { \
        using func_t = retval (*)(__VA_ARGS__); \
        __INTERNAL_HOOK_STRUCT(name_, infoGet, retval, __VA_ARGS__) \
    }; \
    HOOK_AUTO_INSTALL_IMPL(name_, INSTALL_HOOK_ORIG); \
    retval hook_##name_::hook_m_##name_(__VA_ARGS__)
