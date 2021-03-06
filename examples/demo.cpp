/*
 * Copyright (c) 2015 Isode Limited.
 * All rights reserved.
 * See the LICENSE file for more information.
 */

#include <memory>

#include <boost/thread.hpp>

//#include <restpp/drivers/WebSocketPPASIOTLSServerDriver.h> // For TLS
#include <restpp/drivers/WebSocketPPASIOServerDriver.h>
#include <restpp/RESTHandler.h>
#include <restpp/RESTServer.h>


using namespace librestpp;

class StringHandler : public RESTHandler {
	public:
		StringHandler(std::string  result) : result_(std::move(result)) {}
		virtual void handleRequest(std::shared_ptr<RESTRequest> request) override {
			request->setReplyHeader(RESTRequest::HTTP_OK);
			request->setContentType("text/plain");
			request->addReplyContent(result_);
			request->sendReply();
		}

	private:
		std::string result_;
};

class TextEchoHandler : public RESTHandler {
	public:
		TextEchoHandler() {}
		virtual void handleRequest(std::shared_ptr<RESTRequest> request) override {
			request->setReplyHeader(RESTRequest::HTTP_OK);
			request->addReplyContent(request->getBody());
			request->setContentType("text/plain");
			request->sendReply();
		}

};

class JSONEchoHandler : public RESTHandler {
	public:
		JSONEchoHandler() {}
		virtual void handleRequest(std::shared_ptr<RESTRequest> request) override {
			std::shared_ptr<JSONObject> json = request->getJSON();

			request->setReplyHeader(RESTRequest::HTTP_OK);
			if (!!json) {
				request->addReplyContent(json);
				request->setContentType("application/json");
			}
			else {
				request->setContentType("text/plain");
				request->addReplyContent("Invalid JSON sent");
			}
			request->sendReply();
		}

};

class JSONChefHandler : public RESTHandler {
	public:
		JSONChefHandler() {}
		virtual void handleRequest(std::shared_ptr<RESTRequest> request) override {
			std::shared_ptr<JSONObject> json = request->getJSON();

			request->setReplyHeader(RESTRequest::HTTP_OK);
			if (!!json) {
				request->addReplyContent(std::dynamic_pointer_cast<JSONObject>(chefify(json)));
				request->setContentType("application/json");
			}
			else {
				request->setContentType("text/plain");
				request->addReplyContent("Invalid JSON sent");
			}
			request->sendReply();
		}

	private:
		JSONValue::ref chefify(JSONValue::ref value) {
			std::shared_ptr<JSONObject> jsonObject;
			std::shared_ptr<JSONString> jsonString;
			if ((jsonObject = std::dynamic_pointer_cast<JSONObject>(value))) {
				std::shared_ptr<JSONObject> result = std::make_shared<JSONObject>();
				auto values = jsonObject->getValues();
				for (auto & value : values) {
					result->set(value.first, chefify(value.second));
				}
				return result;
			}
			else if ((jsonString = std::dynamic_pointer_cast<JSONString>(value))) {
				return std::make_shared<JSONString>("bork bork");
			}
			return value;
		}
};

int main(int argc, const char* argv[])
{
	// For TLS, use
	// auto driver = std::make_shared<WebSocketPPASIOTLSServerDriver>([](boost::asio::ssl::context* ctx) {});
	// and set up the context in the lambda, from http://www.boost.org/doc/libs/1_58_0/doc/html/boost_asio/reference/ssl__context.html
	auto driver = std::make_shared<WebSocketPPASIOServerDriver>();
	librestpp::RESTServer server(driver);

	std::stringstream longString;
	for (int i = 0; i < 70000; i++) {
		longString << "Arr ";
	}

	std::shared_ptr<RESTHandler> demoHandler = std::make_shared<StringHandler>("hi");
	server.addEndpoint(PathVerb("/demo", PathVerb::GET), demoHandler);

	std::shared_ptr<RESTHandler> longHandler = std::make_shared<StringHandler>(longString.str());
	server.addEndpoint(PathVerb("/long", PathVerb::GET), longHandler);

	std::shared_ptr<RESTHandler> echoGetHandler = std::make_shared<StringHandler>("Echos must be POST");
	server.addEndpoint(PathVerb("/echo", PathVerb::GET), echoGetHandler);

	std::shared_ptr<RESTHandler> echoHandler = std::make_shared<JSONEchoHandler>();
	server.addEndpoint(PathVerb("/echo", PathVerb::POST), echoHandler);

	std::shared_ptr<RESTHandler> textEchoHandler = std::make_shared<TextEchoHandler>();
	server.addEndpoint(PathVerb("/echotext", PathVerb::POST), textEchoHandler);

	std::shared_ptr<RESTHandler> chefHandler = std::make_shared<JSONChefHandler>();
	server.addEndpoint(PathVerb("/chef", PathVerb::POST), chefHandler);

	std::cout << "Starting demo server on port 1080, at paths /demo, /long and /echo (GET) and /echo, /echotext and /chef (POST)" << std::endl;

	driver->listen(1080);
	driver->run();
}
