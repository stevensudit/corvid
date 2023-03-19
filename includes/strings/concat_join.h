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
#include "lite.h"
#include "../enums.h"

// Recommendation: While you can import the entire `corvid::strings` namespace,
// you may not want to bring in all of these symbols, or you may wish to do so
// more selectively.
//
// The way to do that is to import just `corvid` and then reference symbols
// through the `strings` namespace, such as `strings::trim_braces`. You can
// also choose to import the inline namespace for that group of symbols, such
// as `corvid::bracing`.
namespace corvid::strings {
inline namespace registration {

//
// Append plugin
//

// Register an `append` function for a type.
//
// This is the moral equivalent of writing an overload for the `append`
// functions, but has the advantage of actually working. The reason you can't
// just write overloads (of a function that appends a single part) is that the
// various multi-part functions forward to single-part ones, but they can
// only do this to functions that were previously defined. So if you define a
// function later, it does not participate in overload resolution.
//
// The rules for partial specialization of templated values are different and
// more flexible, so this technique takes advantage of them.
//
// Concretely, your function has to fit the signature but is otherwise
// unrestricted. It can be a class static function, a free function, or even a
// lambda.
//
// For example:
//
//    template<AppendTarget A>
//    static auto& append(A& target, const person& p) {
//      return corvid::strings::append(target, p.last, ", ", p.first);
//    }
//
//    template<AppendTarget A>
//    constexpr auto corvid::strings::append_override_fn<A, person> =
//        person::append<A>;
//

// General case to allow specialization.
template<AppendTarget A, typename T>
constexpr auto append_override_fn = nullptr;

// Concept for types that have an overridden appender registered.
template<typename T>
concept AppendableOverridden =
    (!std::is_null_pointer_v<decltype(append_override_fn<std::string, T>)>);

// Concept for types that registered as a stream appendable.
template<typename T>
concept StreamAppendable = stream_append_v<T>;

// Concept for types that are not registered for special handling.
//
// Note that, despite the name, types that fit this concept are only candidates
// for native join appending, not guaranteed to be appendable.
template<typename T>
concept Appendable = (!AppendableOverridden<T>) && (!StreamAppendable<T>);

} // namespace registration

inline namespace appending {

//
// Append, Concat, and Join
//

// The `append`, `append_join`, and `append_join_with` functions take an
// AppendTarget, `target`, which can be a `std::string` or any `std::ostream`,
// as the first parameter and append the rest to it.
//
// The `concat`, `join`, and `join_with` functions take the pieces and return
// the whole as a string.
//
// For the `append_join_with` and `join_with` functions, the parameter right
// after `target` is interpreted as the delimiter to separate the other values
// *with*. The `append_join` and `join` functions instead default the delimiter
// to ", ".
//
// All of the joining functions can have `join_opt` specified to control
// such things as whether container elements are surrounded with appropriate
// braces, whether keys should be shown for containers, whether strings should
// be quoted, and whether a delimiter should be emitted at the start; see enum
// definition below for description.
//
// The supported types for the pieces include: `std::string_view`,
// `std::string`, `const char*`, `char`, `bool`, `int`, `double`, `enum`, and
// containers.
//
// Containers include `std::pair`, `std::tuple`, `std::initializer_list`, and
// anything you can do a ranged-for over, such as `std::vector`. For keyed
// containers, such as `std::map`, only the values are used, unless
// `join_opt` specifies otherwise. Containers may be nested arbitrarily.
//
// In addition to `int` and `double`, all other native numeric types are
// supported.
//
// Pointers and `std::optional` are dereferenced if a value is available. To
// instead show the address of a pointer in hex, cast it to `void*`.
//
// Any other class can be supported by registering an `append_override_fn`
// callback (and, if it needs to support internal delimiters, an
// `append_join_override_fn` callback).
//
// If the class can already stream out to `std::ostream`, you can support it
// for append just by enabling `stream_append_v`.

// Append one stringlike thing to `target`.
// If passed a `const char*` of `nullptr`, it is treated as an empty string.
template<StringViewConvertible T>
requires Appendable<T>
constexpr auto& append(AppendTarget auto& target, const T& part) {
  auto a = appender{target};
  if constexpr (is_char_ptr_v<decltype(part)>)
    a.append(part ? std::string_view{part} : std::string_view{});
  else
    a.append(part);
  return target;
}

// Append one integral number (or `char`) to `target`. When called directly for
// non-char, `base`, `width`, and `pad` may be specified.
template<int base = 10, size_t width = 0, char pad = ' '>
constexpr auto& append(AppendTarget auto& target, std::integral auto part) {
  if constexpr (Char<decltype(part)>)
    appender{target}.append(part);
  else
    append_num<base, width, pad>(target, part);
  return target;
}

// Append one floating-point number to `target`. When called directly, `fmt`,
// `precision`, `width`, and `pad` may be specified.
template<std::chars_format fmt = std::chars_format::general,
    int precision = -1, size_t width = 0, char pad = ' '>
constexpr auto&
append(AppendTarget auto& target, std::floating_point auto part) {
  return append_num<fmt, precision, width, pad>(target, part);
}

// Append one pointer or optional value to `target`. If not present, appends
// empty string.
// TODO: Consider offering a version that allows specifying something other
// than empty string in the case of null.
template<OptionalLike T>
requires Appendable<T>
constexpr auto& append(AppendTarget auto& target, const T& part) {
  if (part) append(target, *part);
  return target;
}

// Append one void pointer, as hex, to `target`.
constexpr auto&
append(AppendTarget auto& target, const VoidPointer auto& part) {
  return append<16>(target, reinterpret_cast<uintptr_t>(part));
}

// Append one scoped or unscoped `enum` to `target`.
//
// Unscoped `enum`s are converted to their underlying type and then appended.
constexpr auto& append(AppendTarget auto& target, const StdEnum auto& part) {
  if constexpr (ScopedEnum<decltype(part)>)
    return append_enum(target, part);
  else
    return append(target, as_underlying(part));
}

// Append one container, as its element values, to `target` without
// delimiters.  See `append_join_with` for delimiter support.  When called
// directly, `keyed` may be specified.
template<bool keyed = false, Container T>
requires Appendable<T>
constexpr auto& append(AppendTarget auto& target, const T& parts) {
  for (auto& part : parts) append(target, element_value<keyed>(part));
  return target;
}

// Apppend one monostate to `target`.
// TODO: Consider offering a version that allows specifying something other
// than empty when valueless.
constexpr auto& append(AppendTarget auto& target, const MonoState auto& part) {
  return target;
}

// Append one variant to `target`, as its current type.
// TODO: Consider offering a version that allows specifying something other
// than empty when valueless.
template<Variant T>
requires Appendable<T>
constexpr auto& append(AppendTarget auto& target, const T& part) {
  if (!part.valueless_by_exception()) {
    std::visit([&target](auto&& inside) { append(target, inside); }, part);
  }
  return target;
}

// Apppend a `StreamAppendable` to `target`.
constexpr auto&
append(AppendTarget auto& target, const StreamAppendable auto& part) {
  append_stream(target, part);
  return target;
}

// Append pieces to `target` without delimiters. See `append_join_with` for
// delimiter support.
constexpr auto& append(AppendTarget auto& target, const auto& head,
    const auto& middle, const auto&... tail) {
  append(append(target, head), middle);
  if constexpr (sizeof...(tail) != 0) append(target, tail...);
  return target;
}

// Append one `std::tuple` or `std::pair`, as its elements, to `target`
// without delimiters. See `append_join_with` for delimiter support.
template<TupleLike T>
requires Appendable<T>
constexpr auto& append(AppendTarget auto& target, const T& parts) {
  std::apply(
      [&target](const auto&... parts) {
        if constexpr (sizeof...(parts) != 0) append(target, parts...);
      },
      parts);
  return target;
}

// Append one append-overridden object to `target`.
//
// Note: The syntax on this is very sensitive, so any change might break it.
template<AppendTarget A, AppendableOverridden T>
constexpr auto& append(A& target, const T& part) {
  auto& fn = strings::append_override_fn<A, T>;
  fn(target, part);
  return target;
}

// Concatenate pieces together into `std::string` without delimiters. See
// `join` and `join_with` for delimiter support.
[[nodiscard]] constexpr auto concat(const auto& head, const auto&... tail) {
  std::string target;
  return append(target, head, tail...);
}

} // namespace appending

inline namespace joinoptions {

//
// Join options
//

// Join option bitmask flags.
enum class join_opt {
  // braced - Show braces around containers; the default behavior.
  braced = 0,
  // flat - Avoid showing braces around containers.
  flat = 1,
  // keyed - Show keys in containers, in addition to values.
  keyed = 2,
  // quoted - Show quotes around strings.
  quoted = 4,
  // prefixed - Prefix with the delimiter.
  prefixed = 8,
  // Convenience aliases:
  // flat-keyed.
  flat_keyed = flat | keyed,
  // json - Show as JSON.
  json = keyed | quoted,
};

// Consider adding a setting that maps to JSON. Perhaps it's just braced +
// keyed + quoted + prefixed?

} // namespace joinoptions
} // namespace corvid::strings

template<>
constexpr size_t ::corvid::enums::bitmask::bit_count_v<
    corvid::strings::joinoptions::join_opt> = 4;

namespace corvid::strings {
inline namespace registration {

//
// Append join plugin
//

// Register an `append_join` function for a type.
//
// See `append_override_fn`, above, for explanations. Note that you may wish to
// use the helpers in the `decode` namespace for more sophisticated
// functionality.
//
// For example:
//
//      template<auto opt = strings::join_opt::braced, char open = 0,
//          char close = 0, AppendTarget A>
//      static A& append_join_with(A& target, strings::delim d,
//          const person& p) {
//        return corvid::strings::append_join_with<opt, open, close>(
//            target, d, p.last, p.first);
//      }
//
//    template<strings::join_opt opt, char open, char close, AppendTarget A>
//        constexpr auto strings::append_join_override_fn<opt, open, close, A,
//        person> = person::append_join_with<opt, open, close, A>;
//
// Note: You will want to specialize `append_override_fn` as well.

// General case to allow specialization.
template<join_opt opt, char open, char close, AppendTarget A, typename T>
constexpr auto append_join_override_fn = nullptr;

// Concept for types that have an overridden join appender registered.
template<typename T>
concept JoinAppendableOverridden =
    (!std::is_null_pointer_v<decltype(append_join_override_fn<join_opt{}, ' ',
            ' ', std::string, T>)>);

// Concept for types that do not have an overridden join appender registered.
//
// Note that, despite the name, types that fit this concept are only candidates
// for native join appending, not guaranteed to be appendable.
template<typename T>
concept JoinAppendable = (!JoinAppendableOverridden<T>) || Appendable<T>;

} // namespace registration
namespace decode {
using namespace corvid::bitmask;

//
// Decode
//

// Determine whether to add braces.
// Logic: Unless braces are suppressed, use braces when we have them.
template<join_opt opt, char open, char close>
constexpr bool braces_v = missing(opt, join_opt::flat) && (open && close);

// Calculate next opt for head part.
// Logic: No need to emit leading delimiter for head, since we've already
// emitted it if it was needed.
template<join_opt opt>
constexpr join_opt head_opt_v = clear(opt, join_opt::prefixed);

// Calculate next opt for next part of tail.
// Logic: After the head, we need to emit leading delimiters before parts.
template<join_opt opt>
constexpr join_opt next_opt_v = set(opt, join_opt::prefixed);

// Determine whether to show keys.
template<join_opt opt>
constexpr bool keyed_v = has(opt, join_opt::keyed);

// Determine whether to show quotes.
template<join_opt opt>
constexpr bool quoted_v = has(opt, join_opt::quoted);

// Determine whether to lead with a delimiter.
template<join_opt opt>
constexpr bool delimit_v = has(opt, join_opt::prefixed);

// Determine whether to show as JSON.
template<join_opt opt>
constexpr bool json_v =
    has_all(opt, join_opt::json) && !has(opt, join_opt::flat);

// Determine whether we need to escape string contents.
template<join_opt opt, char open, char close>
constexpr bool escape_v = quoted_v<opt> && open == '\"' && close == '\"';

} // namespace decode
inline namespace joining {

//
// Join
//
//

// TODO: Write a general comment block explaining what join_opt does and
// doesn't do. In specific, point out that, since we can't know what was
// streamed out before, we can't tell that we need to delimit unless we're
// writing multiple parts ourselves. The caller would have to specify
// delimiting if that's what they want.

// Append one piece to `target`, joining with `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0,
    JoinAppendable T>
requires(!Container<T>) && (!Variant<T>) && (!OptionalLike<T>) &&
        (!TupleLike<T>)
constexpr auto& append_join_with(AppendTarget auto& target, delim d,
    const T& part) {
  constexpr bool add_braces = decode::braces_v<opt, open, close>;
  constexpr bool add_quotes =
      StringViewConvertible<T> && decode::quoted_v<opt>;
  d.append_if<decode::delimit_v<opt>>(target);

  if constexpr (add_braces) append(target, open);
  if constexpr (add_quotes) append(target, '"');

  append(target, part);

  if constexpr (add_quotes) append(target, '"');
  if constexpr (add_braces) append(target, close);
  return target;
}

// Append one pointer or optional value to `target`, joining with `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0,
    OptionalLike T>
requires JoinAppendable<T>
constexpr auto&
append_join_with(AppendTarget auto& target, delim d, const T& part) {
  if (part) append_join_with<opt>(target, d, *part);
  return target;
}

// Append one variant to `target`, as its current type, joining with `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0, Variant T>
requires JoinAppendable<T>
constexpr auto&
append_join_with(AppendTarget auto& target, delim d, const T& part) {
  if (!part.valueless_by_exception()) {
    std::visit(
        [&target, &d](auto&& part) { append_join_with<opt>(target, d, part); },
        part);
  }
  return target;
}

// Append one `std::pair`, as its elements, to `target`, joining with `delim`.
// Supposes join_opt::json by emitting `"key": value`.
template<auto opt = join_opt::braced, char open = 0, char close = 0, StdPair T>
requires JoinAppendable<T>
constexpr auto&
append_join_with(AppendTarget auto& target, delim d, const T& part) {
  if (!decode::keyed_v<opt>)
    return append_join_with<opt, open, close>(target, d, part.second);

  constexpr auto is_json = decode::json_v<opt>;
  constexpr auto head_opt = decode::head_opt_v<opt>;
  constexpr auto next_opt = is_json ? head_opt : decode::next_opt_v<opt>;
  constexpr char next_open = open ? open : (is_json ? 0 : '{');
  constexpr char next_close = close ? close : (is_json ? 0 : '}');
  constexpr bool add_quotes =
      is_json && !StringViewConvertible<decltype(part.first)>;
  // TODO: Should we add !Container and so on?

  d.append_if<decode::delimit_v<opt>>(target);

  constexpr bool add_braces = decode::braces_v<opt, next_open, next_close>;
  if constexpr (add_braces) append(target, next_open);

  constexpr delim dq{"\""};
  dq.append_if<add_quotes>(target);
  append_join_with<head_opt>(target, d, part.first);
  dq.append_if<add_quotes>(target);
  constexpr delim ds{": "};
  ds.append_if<is_json>(target);

  append_join_with<next_opt>(target, d, part.second);

  if constexpr (add_braces) append(target, next_close);

  return target;
}

// Append one `std::tuple`, as its elements, to `target`, joining with `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0,
    StdTuple T>
requires JoinAppendable<T>
constexpr auto&
append_join_with(AppendTarget auto& target, delim d, const T& part) {
  constexpr char next_open = open ? open : '{';
  constexpr char next_close = close ? close : '}';
  std::apply(
      [&target, &d](const auto&... parts) {
        if constexpr (sizeof...(parts) != 0)
          append_join_with<opt, next_open, next_close>(target, d, parts...);
      },
      part);
  return target;
}

// Append one append-overridden object to `target`, joining with `delim`.
//
// Note: The syntax on this is very sensitive, so any change might break it.
template<auto opt = join_opt::braced, char open = 0, char close = 0,
    AppendTarget A, JoinAppendableOverridden T>
constexpr auto& append_join_with(A& target, delim d, const T& part) {
  auto& fn = strings::append_join_override_fn<opt, open, close, A, T>;
  fn(target, d, part);
  return target;
}

// Append one container, as its element values, to `target`, joining with
// `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0,
    Container T>
requires JoinAppendable<T>
constexpr auto&
append_join_with(AppendTarget auto& target, delim d, const T& parts) {
  constexpr auto head_opt = decode::head_opt_v<opt>;
  constexpr auto next_opt = decode::next_opt_v<opt>;
  constexpr bool is_keyed =
      decode::keyed_v<opt> && StdPair<decltype(*cbegin(parts))>;
  constexpr bool is_json = is_keyed && decode::json_v<opt>;
  constexpr char next_open = open ? open : (is_json ? '{' : '[');
  constexpr char next_close = close ? close : (is_json ? '}' : ']');
  constexpr bool add_braces = decode::braces_v<opt, next_open, next_close>;

  d.append_if<decode::delimit_v<opt>>(target);
  if constexpr (add_braces) append(target, next_open);

  if (auto b = std::cbegin(parts), e = std::cend(parts); b != e) {
    append_join_with<head_opt>(target, d, container_element_v<is_keyed>(b));

    for (++b; b != e; ++b)
      append_join_with<next_opt>(target, d, container_element_v<is_keyed>(b));
  }

  if constexpr (add_braces) append(target, next_close);
  return target;
}

namespace details {

// Helper for `append_join_with` parameter pack overload.
template<join_opt opt, char open = 0, char close = 0, typename Head,
    typename... Tail>
constexpr auto& ajwh(AppendTarget auto& target, delim d, const Head& head,
    const Tail&... tail) {
  append_join_with<opt>(target, d, head);
  if constexpr (sizeof...(tail) != 0) ajwh<opt>(target, d, tail...);
  return target;
}
} // namespace details

// Append pieces to `target`, joining with `delim`.
template<auto opt = join_opt::braced, char open = 0, char close = 0>
constexpr auto& append_join_with(AppendTarget auto& target, delim d,
    const auto& head, const auto& middle, const auto&... tail) {
  constexpr bool add_braces = decode::braces_v<opt, open, close>;
  constexpr auto head_opt = decode::head_opt_v<opt>;
  constexpr auto next_opt = decode::next_opt_v<opt>;

  // TODO: Add code to use curly braces if keyed.

  d.append_if<decode::delimit_v<opt>>(target);
  if constexpr (add_braces) append(target, open);

  append_join_with<head_opt>(target, d, head);
  append_join_with<next_opt>(target, d, middle);

  if constexpr (sizeof...(tail) != 0)
    details::ajwh<next_opt>(target, d, tail...);

  if constexpr (add_braces) append(target, close);
  return target;
}

// Append pieces to `target`, joining with a comma and space delimiter.
template<auto opt = join_opt::braced, char open = 0, char close = 0>
constexpr auto&
append_join(AppendTarget auto& target, const auto& head, const auto&... tail) {
  constexpr delim d{", "};
  return append_join_with<opt, open, close>(target, d, head, tail...);
}

// Join pieces together, with `delim`, into `std::string`.
template<auto opt = join_opt::braced, char open = 0, char close = 0>
[[nodiscard]] constexpr auto
join_with(delim d, const auto& head, const auto&... tail) {
  std::string target;
  return append_join_with<opt, open, close>(target, d, head, tail...);
}

// Join pieces together, comma-delimited, into `std::string`.
template<auto opt = join_opt::braced, char open = 0, char close = 0>
[[nodiscard]] constexpr auto join(const auto& head, const auto&... tail) {
  std::string target;
  constexpr delim d{", "sv};
  return append_join_with<opt, open, close>(target, d, head, tail...);
}

} // namespace joining
} // namespace corvid::strings

//
// TODO
//

// TODO: Add method that takes pieces and counts up their total (estimated)
// size, for the purpose of reserving target capacity. Possibly add this an a
// `join_opt` for use in `concat` and `join`.

// TODO: Benchmark delim `find` single-char optimizations, to make sure they're
// faster.

// TODO: Consider offering a way to register a tuple-view function for use in
// `append_join`. The function would return a view of the object's contents as
// a tuple of references (perhaps with the aid of a helper), and the presence
// of this function would pick out the function override. It might also be
// helpful to be able to specify an internal delimiter. It's not clear that any
// of this is useful for straight-up `append`, though.

// TODO: Consider offering the reverse of `stream_append_v`. This would be
// named something like `stream_using_append_v`, and would enable an
// `operator<<` overload for the type, implemented by calling `append` (or
// maybe even `append_join`) on it.