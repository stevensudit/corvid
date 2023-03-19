// Corvid20: A general-purpose C++ 20 library extending std.
// https://github.com/stevensudit/Corvid20
//
// Copyright 2022-2023 Steven Sudit
//
// Licensed under the Apache License, Version 2.0(the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "./meta_shared.h"

namespace corvid::meta {
inline namespace enums {

// Enums

// Cast enum to underlying integer value.
//
// Similar to `std::to_underlying_type` in C++23, but more forgiving. If `T` is
// not an enum, just passes the value through unchanged.
template<typename T>
constexpr auto as_underlying(T v) noexcept {
  if constexpr (std::is_enum_v<T>) {
    return static_cast<std::underlying_type_t<T>>(v);
  } else {
    return v;
  }
}

// Determine underlying type of enum. If not enum, harmlessly returns `T`.
template<typename T>
using as_underlying_t = decltype(as_underlying(std::declval<T>()));

// Cast underlying value to enum.
//
// Similar to `static_cast<T>(U)` except that, when `T` isn't an enum, instead
// returns a default-constructed `X`.
//
// If this seems like a strange thing to want to do, you're not wrong, but it
// turns out to be surprisingly useful.
template<typename T, typename X = std::byte, typename V>
constexpr auto from_underlying(const V& u) {
  if constexpr (ScopedEnum<T>) {
    return static_cast<T>(u);
  } else {
    return X{};
  }
}

} // namespace enums
} // namespace corvid::meta