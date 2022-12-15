//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_TEST_COMMON_HPP
#define BOOST_MYSQL_TEST_COMMON_TEST_COMMON_HPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/auxiliar/make_string_view.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

#include <boost/config.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ostream>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

template <class... Types>
std::vector<field_view> make_fv_vector(Types&&... args)
{
    return std::vector<field_view>{field_view(std::forward<Types>(args))...};
}

inline field_view make_svoff_fv(std::size_t offset, std::size_t size, bool is_blob)
{
    return field_view(detail::string_view_offset(offset, size), is_blob);
}

template <class... Types>
row makerow(Types&&... args)
{
    auto fields = make_field_views(std::forward<Types>(args)...);
    return row(row_view(fields.data(), fields.size()));
}

template <class... Types>
rows makerows(std::size_t num_columns, Types&&... args)
{
    auto fields = make_field_views(std::forward<Types>(args)...);
    return rows(rows_view(fields.data(), fields.size(), num_columns));
}

BOOST_CXX14_CONSTEXPR inline time maket(int hours, int mins, int secs, int micros = 0)
{
    return std::chrono::hours(hours) + std::chrono::minutes(mins) + std::chrono::seconds(secs) +
           std::chrono::microseconds(micros);
}

template <std::size_t N>
constexpr boost::string_view makesv(const char (&value)[N])
{
    return detail::make_string_view(value);
}

template <std::size_t N>
inline boost::string_view makesv(const std::uint8_t (&value)[N])
{
    return boost::string_view(reinterpret_cast<const char*>(value), N);
}

inline boost::string_view makesv(const std::uint8_t* value, std::size_t size)
{
    return boost::string_view(reinterpret_cast<const char*>(value), size);
}

template <std::size_t N>
inline blob_view makebv(const char (&value)[N])
{
    static_assert(N >= 1, "Expected a C-array literal");
    return blob_view(
        reinterpret_cast<const unsigned char*>(value),
        N - 1
    );  // discard null terminator
}

inline void validate_string_contains(std::string value, const std::vector<std::string>& to_check)
{
    std::transform(value.begin(), value.end(), value.begin(), &tolower);
    for (const auto& elm : to_check)
    {
        BOOST_TEST(
            value.find(elm) != std::string::npos,
            "Substring '" << elm << "' not found in '" << value << "'"
        );
    }
}

inline const char* to_string(ssl_mode m)
{
    switch (m)
    {
    case ssl_mode::disable: return "ssldisable";
    case ssl_mode::enable: return "sslenable";
    case ssl_mode::require: return "sslrequire";
    default: assert(false); return "";
    }
}

inline const char* to_string(detail::protocol_field_type t) noexcept
{
    switch (t)
    {
    case detail::protocol_field_type::decimal: return "decimal";
    case detail::protocol_field_type::tiny: return "tiny";
    case detail::protocol_field_type::short_: return "short_";
    case detail::protocol_field_type::long_: return "long_";
    case detail::protocol_field_type::float_: return "float_";
    case detail::protocol_field_type::double_: return "double_";
    case detail::protocol_field_type::null: return "null";
    case detail::protocol_field_type::timestamp: return "timestamp";
    case detail::protocol_field_type::longlong: return "longlong";
    case detail::protocol_field_type::int24: return "int24";
    case detail::protocol_field_type::date: return "date";
    case detail::protocol_field_type::time: return "time";
    case detail::protocol_field_type::datetime: return "datetime";
    case detail::protocol_field_type::year: return "year";
    case detail::protocol_field_type::varchar: return "varchar";
    case detail::protocol_field_type::bit: return "bit";
    case detail::protocol_field_type::newdecimal: return "newdecimal";
    case detail::protocol_field_type::enum_: return "enum_";
    case detail::protocol_field_type::set: return "set";
    case detail::protocol_field_type::tiny_blob: return "tiny_blob";
    case detail::protocol_field_type::medium_blob: return "medium_blob";
    case detail::protocol_field_type::long_blob: return "long_blob";
    case detail::protocol_field_type::blob: return "blob";
    case detail::protocol_field_type::var_string: return "var_string";
    case detail::protocol_field_type::string: return "string";
    case detail::protocol_field_type::geometry: return "geometry";
    default: return "unknown";
    }
}

}  // namespace test

// make protocol_field_type streamable
namespace detail {

inline std::ostream& operator<<(std::ostream& os, protocol_field_type t)
{
    return os << test::to_string(t);
}

}  // namespace detail

}  // namespace mysql
}  // namespace boost

#endif /* TEST_TEST_COMMON_HPP_ */
