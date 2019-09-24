#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "mysql/mysql_stream.hpp"
#include "mysql/prepared_statement.hpp"

using namespace std;
using namespace boost::asio;
using namespace mysql;

constexpr auto HOSTNAME = "localhost"sv;
constexpr auto PORT = "3306"sv;

struct VariantPrinter
{
	template <typename T>
	void operator()(T v) const { cout << v << ", "; }

	void operator()(string_lenenc v) const { (*this)(v.value); }
	void operator()(std::nullptr_t) const { (*this)("NULL"); }
};

template <typename AsyncStream>
void print(mysql::BinaryResultset<AsyncStream>& res)
{
	for (bool ok = res.more_data(); ok; ok = res.retrieve_next())
	{
		for (const auto& field: res.values())
		{
			std::visit(VariantPrinter(), field);
		}
		std::cout << "\n";
	}
	const auto& ok = res.ok_packet();
	std::cout << "affected_rows=" << ok.affected_rows.value
			  << ", last_insert_id=" << ok.last_insert_id.value
			  << ", warnings=" << ok.warnings
			  << ", info=" << ok.info.value << endl;
}

int main_sync()
{
	// Basic
	io_context ctx;
	auto guard = make_work_guard(ctx);
	boost::system::error_code errc;

	// DNS resolution
	ip::tcp::resolver resolver {ctx};
	auto results = resolver.resolve(ip::tcp::v4(), HOSTNAME, PORT);
	if (results.size() != 1)
	{
		cout << "Found endpoints: " << results.size() << ", exiting" << endl;
		exit(1);
	}
	auto endpoint = results.begin()->endpoint();
	cout << "Connecting to: " << endpoint << endl;

	// MYSQL stream
	MysqlStream<boost::asio::ip::tcp::socket> stream {ctx};

	// TCP connection
	stream.next_layer().connect(endpoint);

	// Handshake
	stream.handshake(mysql::HandshakeParams{
		CharacterSetLowerByte::utf8_general_ci,
		"root",
		"root",
		"awesome"
	});

	// Prepare a statement
	auto stmt = mysql::PreparedStatement<boost::asio::ip::tcp::socket>::prepare(
			stream, "SELECT * from users WHERE age < ? and first_name <> ?");
	auto res = stmt.execute_with_cursor(2, 200, string_lenenc{"hola"});
	print(res);
	auto make_older = mysql::PreparedStatement<boost::asio::ip::tcp::socket>::prepare(
			stream, "UPDATE users SET age = age + 1");
	res = make_older.execute();
	print(res);
	make_older.close();
	res = stmt.execute_with_cursor(8, 70, string_lenenc{"hola"});
	cout << "\n\n";
	print(res);

	return 0;
}

int main()
{
	// Basic
	io_context ctx;
	auto guard = make_work_guard(ctx);
	boost::system::error_code errc;

	// DNS resolution
	ip::tcp::resolver resolver {ctx};
	auto results = resolver.resolve(ip::tcp::v4(), HOSTNAME, PORT);
	if (results.size() != 1)
	{
		cout << "Found endpoints: " << results.size() << ", exiting" << endl;
		exit(1);
	}
	auto endpoint = results.begin()->endpoint();
	cout << "Connecting to: " << endpoint << endl;

	// MYSQL stream
	MysqlStream<boost::asio::ip::tcp::socket> stream {ctx};

	// TCP connection
	stream.next_layer().connect(endpoint);

	// Handshake
	mysql::HandshakeParams handshake_params {
			CharacterSetLowerByte::utf8_general_ci,
			"root",
			"root",
			"awesome"
	};

	stream.async_handshake(
			handshake_params,
			[&guard] { cout << "Connected to server in async mode" << endl; guard.reset(); }
	);

	ctx.run();

	std::cout << "Finished, returning" << endl;

}
