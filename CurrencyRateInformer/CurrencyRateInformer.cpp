
#include <iostream>

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/json.hpp>

/*
  Get body from website

  @param[in] host - Website address.
  @param[in] args - Arguments.

  @retval Task with the content of the response from the site without headers.
*/
boost::asio::awaitable<std::string> get_request_async(std::string host, std::string args = "/")
{
	auto executor = co_await boost::asio::this_coro::executor;

	boost::asio::ip::tcp::resolver resolver(executor);
	boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket(executor, ctx);

	auto &socket = ssl_socket.lowest_layer();
	auto endpoints = co_await resolver.async_resolve(host, "443", boost::asio::use_awaitable);

	co_await boost::asio::async_connect(socket, endpoints, boost::asio::use_awaitable);
	co_await ssl_socket.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);

	boost::beast::http::request<boost::beast::http::string_body> req(boost::beast::http::verb::get, args, 11);
	req.set(boost::beast::http::field::host, host);

	co_await boost::beast::http::async_write(ssl_socket, req, boost::asio::use_awaitable);

	boost::beast::http::response<boost::beast::http::string_body> res;
	boost::beast::flat_buffer input_buffer;

	co_await boost::beast::http::async_read(ssl_socket, input_buffer, res, boost::asio::use_awaitable);

	co_return std::move(res.body());
}

/*
  Parsing the response from the server.

  @param[in] ioc - Executor.
  @param[in] exception - Exception thrown from token.
  @param[in] response - Response from the server to parse.
*/
void parse_response(boost::asio::io_context &ioc, const std::exception_ptr &exception, std::string_view response)
{
	if (exception)
	{
		std::rethrow_exception(exception);
	}

	std::stringstream output;

	auto info = boost::json::parse(response);
	auto rates = info.at("rates");

	output << std::format("BSD | {}\n", rates.at("BSD").to_number<double>());
	output << std::format("BTC | {}\n", rates.at("BTC").to_number<double>());
	output << std::format("BTN | {}\n", rates.at("BTN").to_number<double>());
	output << std::format("EUR | {}\n", rates.at("EUR").to_number<double>());
	output << std::format("PEN | {}\n", rates.at("PEN").to_number<double>());
	output << std::format("RUB | {}\n", rates.at("RUB").to_number<double>());
	output << std::format("STD | {}\n", rates.at("STD").to_number<double>());

	output << "===================================================\n";
	output << std::format("Data acquisition time | {}\n", std::chrono::system_clock::now());

	std::system("cls");
	std::cout << output.str();

	ioc.stop();
}

/*
  Program entry point.

  @param[in] argc - Number of arguments.
  @param[in] argv - Arguments.

  @retval Execution status.
*/
int main()
{
	try
	{
		while (true)
		{
			boost::asio::io_context ioc;

			boost::asio::co_spawn(ioc, get_request_async("openexchangerates.org", "/api/latest.json?app_id=cb93f581065d42688b495b9011c962ad"),
				std::bind(parse_response, std::ref(ioc), std::placeholders::_1, std::placeholders::_2));

			ioc.run();

			std::this_thread::sleep_for(std::chrono::minutes(1));
		}
	}
	catch (const std::exception &ex)
	{
		std::setlocale(0, "");
		std::cout << ex.what() << std::endl;
	}
}
