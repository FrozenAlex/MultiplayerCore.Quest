#pragma once
#include "System/Type.hpp"
#include "System/Array.hpp"
#include "beatsaber-hook/shared/arrayw.hpp"
#include <algorithm>
#include <cctype>
#include <string>
namespace MultiplayerCore::Utils {
struct EnumUtils {
    template<class T> static StringW GetEnumName(T value) {
        System::Type* type = i2c::cs_type_of<T>();
        return type->GetEnumName(reinterpret_cast<System::Object*>(i2c::functions::value_box(i2c::class_of<T>(), &value)));
    }
    template<class T> static T GetEnumValue(const std::string& name, bool ignoreCase = false) {
        System::Type* type = i2c::cs_type_of<T>();
        auto names = type->GetEnumNames();
        auto values = type->GetEnumValues();
        for (int i = 0; i < names.size(); ++i) {
            std::string candidate(names[i]);
            bool match = ignoreCase ? std::equal(candidate.begin(), candidate.end(), name.begin(), name.end(),
                [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }) : candidate == name;
            if (match) return *static_cast<T*>(i2c::functions::object_unbox(reinterpret_cast<Il2CppObject*>(values->GetValue(i))));
        }
        return T(0);
    }
};
}
