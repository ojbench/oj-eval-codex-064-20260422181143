#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <vector>

namespace sjtu {

using sv_t = std::string_view;

struct format_error : std::exception {
public:
    format_error(const char *msg = "invalid format") : msg(msg) {}
    auto what() const noexcept -> const char * override { return msg; }

private:
    const char *msg;
};

template <typename Tp>
struct formatter;

struct format_info {
    inline static constexpr auto npos = static_cast<std::size_t>(-1);
    std::size_t position; // where is the specifier
    std::size_t consumed; // how many characters consumed
};

// Find next non-escaped specifier "%X"; advances fmt to the char after '%'
consteval auto find_specifier(sv_t &fmt) -> bool {
    do {
        if (const auto next = fmt.find('%'); next == sv_t::npos) {
            return false;
        } else if (next + 1 == fmt.size()) {
            throw format_error{"missing specifier after '%'"};
        } else if (fmt[next + 1] == '%') {
            // escape the specifier
            fmt.remove_prefix(next + 2);
        } else {
            fmt.remove_prefix(next + 1);
            return true;
        }
    } while (true);
};

template <typename... Args>
struct format_string {
public:
    // must be constructed at compile time, to ensure the format string is valid
    consteval format_string(const char *fmt);
    constexpr auto get_format() const -> std::string_view { return fmt_str; }
    constexpr auto get_index() const -> std::span<const format_info> { return fmt_idx; }

private:
    inline static constexpr auto Nm = sizeof...(Args);
    std::string_view fmt_str;            // the format string
    std::array<format_info, Nm> fmt_idx; // where are the specifiers
};

template <typename Parser>
consteval auto parse_one(sv_t &fmt, std::size_t &n, std::span<format_info> idx, const Parser &parser) -> void {
    const auto last_pos = fmt.begin();
    if (!find_specifier(fmt)) {
        // no specifier found
        idx[n++] = { .position = format_info::npos, .consumed = 0 };
        return;
    }

    const auto position = static_cast<std::size_t>(fmt.begin() - last_pos - 1);
    const auto consumed = parser.parse(fmt);

    idx[n++] = { .position = position, .consumed = consumed };

    if (consumed > 0) {
        fmt.remove_prefix(consumed);
    } else if (fmt.starts_with("_")) {
        fmt.remove_prefix(1);
    } else {
        throw format_error{"invalid specifier"};
    }
}

template <typename... Args>
consteval auto compile_time_format_check(sv_t fmt, std::span<format_info> idx) -> void {
    std::size_t n = 0;
    (parse_one(fmt, n, idx, formatter<Args>{}), ...);
    if (find_specifier(fmt)) // extra specifier found
        throw format_error{"too many specifiers"};
}

template <typename... Args>
consteval format_string<Args...>::format_string(const char *fmt) : fmt_str(fmt), fmt_idx() {
    compile_time_format_check<Args...>(fmt_str, fmt_idx);
}

// String-like formatters (%s)
template <typename StrLike>
    requires(
        std::same_as<std::decay_t<StrLike>, std::string> ||
        std::same_as<std::decay_t<StrLike>, std::string_view> ||
        std::same_as<std::decay_t<StrLike>, char *> ||
        std::same_as<std::decay_t<StrLike>, const char *>)
struct formatter<StrLike> {
    static constexpr auto parse(sv_t fmt) -> std::size_t { return fmt.starts_with("s") ? 1 : 0; }
    static auto format_to(std::ostream &os, const StrLike &val, sv_t fmt = "s") -> void {
        if (fmt.starts_with("s")) {
            if constexpr (std::same_as<std::decay_t<StrLike>, std::string_view>) {
                os << val;
            } else if constexpr (std::same_as<std::decay_t<StrLike>, std::string>) {
                os << std::string_view(val);
            } else { // char* or const char*
                os << sv_t(val ? val : "");
            }
        } else {
            throw format_error{};
        }
    }
};

// Signed integer formatter (%d)
template <typename T>
    requires(std::signed_integral<std::decay_t<T>>)
struct formatter<T> {
    static constexpr auto parse(sv_t fmt) -> std::size_t { return fmt.starts_with("d") ? 1 : 0; }
    static auto format_to(std::ostream &os, const T &val, sv_t fmt = "d") -> void {
        if (fmt.starts_with("d")) {
            os << static_cast<long long>(val);
        } else {
            throw format_error{};
        }
    }
};

// Unsigned integer formatter (%u)
template <typename T>
    requires(std::unsigned_integral<std::decay_t<T>>)
struct formatter<T> {
    static constexpr auto parse(sv_t fmt) -> std::size_t { return fmt.starts_with("u") ? 1 : 0; }
    static auto format_to(std::ostream &os, const T &val, sv_t fmt = "u") -> void {
        if (fmt.starts_with("u")) {
            os << static_cast<unsigned long long>(val);
        } else {
            throw format_error{};
        }
    }
};

// Default formatting helper for %_
template <typename T>
inline auto default_format(std::ostream &os, const T &value) -> void {
    if constexpr (std::same_as<std::decay_t<T>, std::string> ||
                  std::same_as<std::decay_t<T>, std::string_view> ||
                  std::same_as<std::decay_t<T>, const char *> ||
                  std::same_as<std::decay_t<T>, char *>) {
        formatter<T>::format_to(os, value, "s");
    } else if constexpr (std::signed_integral<std::decay_t<T>>) {
        formatter<T>::format_to(os, value, "d");
    } else if constexpr (std::unsigned_integral<std::decay_t<T>>) {
        formatter<T>::format_to(os, value, "u");
    } else if constexpr (requires { typename std::decay_t<T>::value_type; } &&
                         std::same_as<std::decay_t<T>, std::vector<typename std::decay_t<T>::value_type>>) {
        using Elem = typename std::decay_t<T>::value_type;
        os << '[';
        for (std::size_t i = 0; i < value.size(); ++i) {
            default_format<Elem>(os, value[i]);
            if (i + 1 != value.size()) os << ',';
        }
        os << ']';
    } else {
        // Fallback: try stream insertion
        os << value;
    }
}

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

// Dispatch printing of the idx-th argument
template <typename Tuple>
inline auto dispatch_arg(std::ostream &os, Tuple &tup, std::size_t idx, char spec) -> void {
    constexpr std::size_t N = std::tuple_size_v<std::decay_t<Tuple>>;
    bool handled = false;
    auto call_default = [&](auto &v) {
        if (spec == '_') default_format(os, v);
        else if (spec == 's') formatter<std::decay_t<decltype(v)>>::format_to(os, v, "s");
        else if (spec == 'd') formatter<std::decay_t<decltype(v)>>::format_to(os, v, "d");
        else if (spec == 'u') formatter<std::decay_t<decltype(v)>>::format_to(os, v, "u");
        else throw format_error{"invalid specifier"};
    };

    // Unroll over indices
    auto try_index = [&](auto Iconst) {
        constexpr std::size_t I = decltype(Iconst)::value;
        if (!handled && idx == I) {
            call_default(std::get<I>(tup));
            handled = true;
        }
    };

    // Generate indices at compile-time
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (try_index(std::integral_constant<std::size_t, Is>{}), ...);
    }(std::make_index_sequence<N>{});

    if (!handled) throw format_error{"argument index out of range"};
}

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    auto s = fmt.get_format();
    std::tuple<const Args &...> tup(args...);
    std::ostream &os = std::cout;

    std::size_t arg_idx = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c != '%') {
            os << c;
            continue;
        }
        if (i + 1 >= s.size()) throw format_error{"missing specifier after '%'"};
        char spec = s[i + 1];
        if (spec == '%') { // escaped percent
            os << '%';
            ++i; // skip next char
            continue;
        }
        // spec must be one of s/d/u/_
        if (spec != 's' && spec != 'd' && spec != 'u' && spec != '_') {
            throw format_error{"invalid specifier"};
        }
        dispatch_arg(os, tup, arg_idx, spec);
        ++arg_idx;
        ++i; // skip spec char
    }
    if constexpr (sizeof...(Args) != 0) {
        if (arg_idx != sizeof...(Args)) throw format_error{"too few specifiers"};
    }
}

} // namespace sjtu
