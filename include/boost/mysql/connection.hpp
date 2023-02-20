//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/auxiliar/rebind_executor.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>

#include <boost/config/pragma_message.hpp>

#include <type_traits>
#include <utility>

#ifndef BOOST_MYSQL_EXPERIMENTAL_API
BOOST_PRAGMA_MESSAGE(
    "Boost.MySQL API is still experimental. The functions connection::query() and "
    "connection::execute_statement() might change in subsequent releases. Define "
    "BOOST_MYSQL_EXPERIMENTAL_API to suppress this warning"
)
#endif

/// The Boost libraries namespace.
namespace boost {
/// Boost.Mysql library namespace.
namespace mysql {

/**
 * \brief A connection to a MySQL server.
 *
 * \details
 * Represents a connection to a MySQL server.
 *\n
 * `connection` is the main I/O object that this library implements. It owns a `Stream` object that
 * is accessed by functions involving network operations, as well as session state. You can access
 * the stream using \ref connection::stream, and its executor via \ref connection::get_executor. The
 * executor used by this object is always the same as the underlying stream.
 */
template <class Stream>
class connection
{
    std::unique_ptr<detail::channel<Stream>> channel_;

    const detail::channel<Stream>& get_channel() const noexcept
    {
        assert(channel_ != nullptr);
        return *channel_;
    }
    diagnostics& shared_diag() noexcept { return get_channel().shared_diag(); }

    detail::channel<Stream>& get_channel() noexcept
    {
        assert(channel_ != nullptr);
        return *channel_;
    }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::connection_access;
#endif

public:
    /**
     * \brief Initializing constructor.
     * \details
     * As part of the initialization, a `Stream` object is created
     * by forwarding any passed in arguments to its constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(Args&&... args) : connection(buffer_params(), std::forward<Args>(args)...)
    {
    }

    /**
     * \brief Initializing constructor with buffer params.
     * \details
     * As part of the initialization, a `Stream` object is created
     * by forwarding any passed in arguments to its constructor.
     *\n
     * The internal connection buffers will be allocated according to `buff_params`.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(const buffer_params& buff_params, Args&&... args)
        : channel_(new detail::channel<Stream>(buff_params.initial_read_size(), std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Move constructor.
     * \details \ref statement objects referencing `other` remain usable for I/O operations.
     */
    connection(connection&& other) = default;

    /**
     * \brief Move assignment.
     * \details \ref statement objects referencing `other` remain usable for I/O operations.
     * Statements referencing `*this` will no longer be usable.
     */
    connection& operator=(connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
#endif

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return get_channel().get_executor(); }

    /// The `Stream` type this connection is using.
    using stream_type = Stream;

    /// Retrieves the underlying Stream object.
    Stream& stream() { return get_channel().stream().next_layer(); }

    /// Retrieves the underlying Stream object.
    const Stream& stream() const { return get_channel().stream().next_layer(); }

    /**
     * \brief Returns whether the connection uses SSL or not.
     * \details This function always returns `false` if the underlying
     * stream does not support SSL. This function always returns `false`
     * for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *\n
     * This function can be used to determine whether you are using a SSL
     * connection or not when using SSL negotiation.
     */
    bool uses_ssl() const noexcept { return get_channel().ssl_active(); }

    /**
     * \brief Returns the current metadata mode that this connection is using.
     */
    metadata_mode meta_mode() const noexcept { return get_channel().meta_mode(); }

    /**
     * \brief Sets the metadata mode.
     * \details
     * Will affect any query and statement executions performed after the call.
     *\n
     * Calling this function while an asynchronous query or statement execution
     * is outstanding causes unspecified results.
     */
    void set_meta_mode(metadata_mode v) noexcept { get_channel().set_meta_mode(v); }

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details This function is only available if `Stream` satisfies the
     * `SocketStream` concept.
     *\n
     * Connects the underlying stream and performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to \ref connection::handshake.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& ec,
        diagnostics& diag
    );

    /// \copydoc connect
    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const handshake_params& params);

    /**
     * \copydoc connect
     *\n
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_connect(endpoint, params, this->shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_connect
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Performs the MySQL-level handshake.
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::connect instead of this function.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void handshake(const handshake_params& params, error_code& ec, diagnostics& diag);

    /// \copydoc handshake
    void handshake(const handshake_params& params);

    /**
     * \copydoc handshake
     *\n
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_handshake(params, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_handshake
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Executes a SQL text query.
     * \details
     * Sends `query_string` to the server for execution and reads the response into `result`.
     * `query_string` should be encoded using the connection's character set.
     *\n
     * After this operation completes successfully, `result.has_value() == true`.
     *\n
     * Metadata in `result` will be populated according to `this->meta_mode()`.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void query(string_view query_string, results& result, error_code&, diagnostics&);

    /// \copydoc query
    void query(string_view query_string, results& result);

    /**
     * \copydoc query
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` __must be kept alive by the caller until the operation is
     * initiated__.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        results& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(query_string, result, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        results& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a text query as a multi-function operation.
     * \details Writes the query request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     * Metadata will be populated according to `this->meta_mode()`.
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using
     * \ref read_some_rows) before engaging in any further operation involving network reads.
     * Otherwise, the results are undefined.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * `query_string` should be encoded using the connection's character set.
     */
    void start_query(string_view query_string, execution_state& st, error_code&, diagnostics&);

    /// \copydoc start_query
    void start_query(string_view query_string, execution_state& st);

    /**
     * \copydoc start_query
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` <b>must be kept alive by the caller until the operation is
     * initiated</b>.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_query(
        string_view query_string,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_query(query_string, st, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_start_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_query(
        string_view query_string,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement server-side.
     * \details
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * `stmt` should be encoded using the connection's character set.
     */
    statement prepare_statement(string_view stmt, error_code&, diagnostics&);

    /// \copydoc prepare_statement
    statement prepare_statement(string_view stmt);

    /**
     * \copydoc prepare_statement
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `stmt` <b>must be kept alive by the caller until the operation is
     * initiated</b>.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code, boost::mysql::statement)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, statement))
    async_prepare_statement(
        string_view stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(stmt, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, statement))
    async_prepare_statement(
        string_view stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Executes a prepared statement.
     * \details
     * Executes the statement with the given parameters and reads the response into `result`.
     *\n
     * After this operation completes successfully, `result.has_value() == true`.
     *\n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the `FieldLikeTuple` concept defition for more info. You should pass exactly as many
     * parameters as `this->num_params()`, or the operation will fail with an error.
     * String parameters should be encoded using the connection's character set.
     *\n
     * Metadata in `result` will be populated according to `conn.meta_mode()`, where `conn`
     * is the connection that prepared this statement.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute_statement(
        const statement& stmt,
        const FieldLikeTuple& params,
        results& result,
        error_code& err,
        diagnostics& diag
    );

    /// \copydoc execute_statement
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute_statement(const statement& stmt, const FieldLikeTuple& params, results& result);

    /**
     * \copydoc execute_statement
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const statement& stmt,
        FieldLikeTuple&& params,
        results& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute_statement(
            stmt,
            std::forward<FieldLikeTuple>(params),
            result,
            shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute_statement
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const statement& stmt,
        FieldLikeTuple&& params,
        results& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a statement execution as a multi-function operation.
     * \details
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     * Metadata will be populated according to `conn.meta_mode()`, where `conn`
     * is the connection that prepared this statement.
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using
     * \ref read_some_rows) before engaging in any further
     * operation involving server communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the `FieldLikeTuple` concept defition for more info. You should pass exactly as many
     * parameters as `this->num_params()`, or the operation will fail with an error.
     * String parameters should be encoded using the connection's character set.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void start_statement_execution(
        const statement& stmt,
        const FieldLikeTuple& params,
        execution_state& ex,
        error_code& err,
        diagnostics& diag
    );

    /// \copydoc start_statement_execution(const statement&,const FieldLikeTuple&,execution_state&,error_code&,diagnostics&)
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void start_statement_execution(const statement& stmt, const FieldLikeTuple& params, execution_state& st);

    /**
     * \copydoc start_statement_execution(const statement&,const FieldLikeTuple&,execution_state&,error_code&,diagnostics&)
     * \details
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldLikeTuple&& params,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_statement_execution(
            stmt,
            std::forward<FieldLikeTuple>(params),
            st,
            get_channel().shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_statement_execution(const statement&,FieldLikeTuple&&,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldLikeTuple&& params,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a statement execution as a multi-function operation.
     * \details
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using
     * \ref connection::read_some_rows) before engaging in any further
     * operation involving server communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters are passed as an iterator range.
     * See the `FieldViewForwardIterator` concept defition for more info. You should pass exactly as
     * many parameters as `this->num_params()`, or the operation will fail with an error. String
     * parameters should be encoded using the connection's character set.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        error_code& ec,
        diagnostics& diag
    );

    /// \copydoc start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,diagnostics&)
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st
    );

    /**
     * \copydoc start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,diagnostics&)
     * \details
     * If `CompletionToken` is deferred (like `use_awaitable`), the caller must keep objects in
     * the iterator range alive until the  operation is initiated.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_statement_execution(
            stmt,
            params_first,
            params_last,
            st,
            get_channel().shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes a statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `this->valid()` will return `false`, and no further functions
     * may be called on this prepared statement, other than assignment.
     *\n
     * This operation involves only writes on the underlying stream.
     */
    void close_statement(const statement& stmt, error_code&, diagnostics&);

    /// \copydoc close_statement
    void close_statement(const statement& stmt);

    /**
     * \copydoc close_statement
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close_statement(
        const statement& stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_close_statement(stmt, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close_statement(
        const statement& stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. If the resultset being read
     * has still rows to read, at least one will be read. If there are no more
     * rows to be read, returns an empty `rows_view`.
     * \n
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * The returned view points into memory owned by `*this`. It will be valid until the
     * underlying stream performs any other read operation or is destroyed.
     *\n
     * This operation involves only reads on the underlying stream.
     */
    rows_view read_some_rows(execution_state& st, error_code& err, diagnostics& info);

    /// \copydoc read_some_rows
    rows_view read_some_rows(execution_state& st);

    /**
     * \copydoc read_some_rows
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::rows_view)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some_rows(
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some_rows(st, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_some_rows
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some_rows(
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Checks whether the server is alive.
     * \details
     * If the server is alive, this function will complete without error.
     * If it's not, it will fail with the relevant network or protocol error.
     *\n
     * Note that ping requests are treated as any other type of request at the protocol
     * level, and won't be prioritized anyhow by the server. If the server is stuck
     * in a long-running query, the ping request won't be answered until the query is
     * finished.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void ping(error_code&, diagnostics&);

    /// \copydoc ping
    void ping();

    /**
     * \copydoc ping
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_ping(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_ping(this->shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_ping
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_ping(diagnostics& diag, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    /**
     * \brief Closes the connection to the server.
     * \details
     * This function is only available if `Stream` satisfies the `SocketStream` concept.
     *\n
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to \ref connection::quit.
     *\n
     * After calling this function, any \ref statement referencing `*this` will
     * no longer be usable for server interaction.
     *\n
     */
    void close(error_code&, diagnostics&);

    /// \copydoc close
    void close();

    /**
     * \copydoc close
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_close(this->shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Notifies the MySQL server that the client wants to end the session and shutdowns SSL.
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should
     * close the underlying physical connection after calling this function.
     *\n
     * After calling this function, any \ref statement referencing `*this` will
     * no longer be usable for server interaction.
     *\n
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::close instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit(error_code&, diagnostics&);

    /// \copydoc quit
    void quit();

    /**
     * \copydoc quit
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_quit(shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_quit
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(diagnostics& diag, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    /**
     * \brief Rebinds the connection type to another executor.
     * \details The Stream type must either provide a `rebind_executor`
     * member with the same semantics, or be an instantiation of `boost::asio::ssl::stream` with
     * a Stream type providing a `rebind_executor` member.
     */
    template <typename Executor1>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<typename detail::rebind_executor<Stream, Executor1>::type>;
    };
};

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

/// The default TCP port for the MySQL protocol, as a string. Useful for hostname resolution.
constexpr const char* default_port_string = "3306";

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/connection.hpp>

#endif
