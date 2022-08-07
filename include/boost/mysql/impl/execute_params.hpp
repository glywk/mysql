//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_EXECUTE_PARAMS_HPP
#define BOOST_MYSQL_IMPL_EXECUTE_PARAMS_HPP

#pragma once

#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <stdexcept>

namespace boost {
namespace mysql {
namespace detail {

template <class ValueForwardIterator>
void check_num_params(
    ValueForwardIterator first,
    ValueForwardIterator last,
    const prepared_statement& stmt
)
{
    auto param_count = std::distance(first, last);
    if (param_count != stmt.num_params())
    {
        throw std::domain_error(detail::stringize(
                "prepared_statement::execute: expected ", stmt.num_params(), " params, but got ", param_count));
    }
}

} // detail
} // mysql
} // boost

template <class ValueForwardIterator>
boost::mysql::execute_params<ValueForwardIterator>::execute_params(
    const prepared_statement& stmt,
    ValueForwardIterator first,
    ValueForwardIterator last
) :
    statement_id_(stmt.id()),
    first_(first),
    last_(last)
{
    detail::check_num_params(first, last, stmt);
}


#endif
