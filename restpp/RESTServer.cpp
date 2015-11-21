/*
 * Copyright (c) 2015 Isode Limited.
 * All rights reserved.
 * See the LICENSE file for more information.
 */

#include "RESTServer.h"

#include <memory>
#include <sstream>

#include <boost/bind.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/http/constants.hpp>


#include "JSONRESTHandler.h"

namespace librestpp {

class RESTRequestInt : public RESTRequest {
	public:
		RESTRequestInt(const PathVerb& pathVerb, const std::string& body, websocketpp::server<websocketpp::config::asio>::connection_ptr connection) : RESTRequest(pathVerb, body), connection_(std::move(connection)), contentType_("application/octet-stream") {

		}

		void setReplyHeader(RESTRequest::ResultCode code) override {
			connection_->set_status(ourCodeToTheirCode(code));
		}

		void addReplyContent(const std::string& content) override {
			//TODO: When websocketpp supports partial responses, support them
			//TODO: Binary responses
			reply_ << content;
		}

		void sendReply() override {
			connection_->replace_header("Content-Type", contentType_);
			connection_->set_body(reply_.str());
#ifndef RESTPP_NO_DEFER
			/*websocketpp::lib::error_code ec = */connection_->send_http_response();
#endif
		}

		void setContentType(const std::string& contentType) override {
			contentType_ = contentType;
		}

		boost::optional<std::string> getHeader(const std::string& header) override {
			boost::optional<std::string> result;
			std::string value = connection_->get_request_header(header);
			if (!value.empty()) {
				result = value;
			}
			return result;
		}

	private:

		websocketpp::http::status_code::value ourCodeToTheirCode(RESTRequest::ResultCode code) {
			switch (code) {
				//FIXME: More codes here
				case RESTRequest::HTTP_OK: return websocketpp::http::status_code::ok;
				case RESTRequest::HTTP_UNAUTHORIZED: return websocketpp::http::status_code::unauthorized;
				default: return websocketpp::http::status_code::not_found;
			}
		}

		websocketpp::server<websocketpp::config::asio>::connection_ptr connection_;
		std::stringstream reply_;
		std::string contentType_;
};

class WebSocketInt : public WebSocket {
	public:
		WebSocketInt(websocketpp::server<websocketpp::config::asio>::connection_ptr connection, websocketpp::server<websocketpp::config::asio>* server) : connection_(connection), server_(server) {
			connection->set_message_handler(boost::bind(&WebSocketInt::handleMessageInt, this, _2));
			connection->set_close_handler(boost::bind(&WebSocketInt::handleClosedInt, this));
			connection->set_fail_handler(boost::bind(&WebSocketInt::handleClosedInt, this));
		}

		void send(const std::string& message) override {
			server_->send(connection_, message, websocketpp::frame::opcode::text);
		}

	private:
		void handleMessageInt(websocketpp::server<websocketpp::config::asio>::message_ptr message) {
			handleMessage(message->get_payload());
		}

		void handleClosedInt() {
			onClosed();
		}
	private:
		websocketpp::server<websocketpp::config::asio>::connection_ptr connection_;
		websocketpp::server<websocketpp::config::asio>* server_;
};

class RESTServer::Private {
	public:
		Private(std::shared_ptr<boost::asio::io_service> ioService) : ioService_(std::move(ioService)) {
			server_.set_http_handler(boost::bind(&RESTServer::Private::handleHTTPRequest, this, _1));
			server_.set_open_handler(boost::bind(&RESTServer::Private::handleNewWebSocket,this, _1));
		}

		bool start(int port) {
			try {
				if (!!ioService_) {
					server_.init_asio(ioService_.get());
				}
				else {
					server_.init_asio();
				}
				server_.listen(port);
				server_.start_accept();
			}
			catch (const websocketpp::exception& /*e*/) {
				return false;
			}
			return true;
		}

		void setReuseAddr(bool b) {
			server_.set_reuse_addr(b);
		}

		void addJSONEndpoint(const PathVerb& pathVerb, std::shared_ptr<JSONRESTHandler> handler) {
			handlers_[pathVerb] = handler;
		}

		void addDefaultGetEndpoint(std::shared_ptr<JSONRESTHandler> handler) {
			defaultHandler_ = handler;
		}

		void poll() {
			server_.poll();
		}

		void run() {
			server_.run();
		}

		boost::signals2::signal<void(std::shared_ptr<WebSocket>)> onWebSocketConnection;

	private:
		void handleNewWebSocket(websocketpp::connection_hdl handle) {
			websocketpp::server<websocketpp::config::asio>::connection_ptr connection = server_.get_con_from_hdl(handle);
			WebSocket::ref webSocket = std::make_shared<WebSocketInt>(connection, &server_);
			onWebSocketConnection(webSocket);
		}

		void handleHTTPRequest(websocketpp::connection_hdl handle) {
			websocketpp::server<websocketpp::config::asio>::connection_ptr connection = server_.get_con_from_hdl(handle);
			std::string path = connection->get_uri()->get_resource();
			std::string method = connection->get_request().get_method();
			PathVerb::RESTVerb verb = PathVerb::INVALID;
			if (method == "GET") {
				verb = PathVerb::GET;
			}
			else if (method == "POST") {
				verb = PathVerb::POST;
			}
			//FIXME: Other verbs

			//TODO: Check resource exhaustion
			std::string body = connection->get_request_body();
			PathVerb pathVerb(path, verb);
			std::shared_ptr<RESTRequest> request = std::make_shared<RESTRequestInt>(pathVerb, body, connection);

			std::shared_ptr<JSONRESTHandler> handler = handlers_[pathVerb];
			if (verb != PathVerb::INVALID && (!!handler || !!defaultHandler_)) {
				if (!handler) {
					handler = defaultHandler_;
				}
#ifndef RESTPP_NO_DEFER
				/*websocketpp::lib::error_code ec =*/connection->defer_http_response();
#endif
				handler->handleRequest(request);
			}
			else {
				connection->set_body("Not found");
				connection->set_status(websocketpp::http::status_code::not_found);
			}
		}

		std::shared_ptr<boost::asio::io_service> ioService_;
		websocketpp::server<websocketpp::config::asio> server_;
		std::map<PathVerb, std::shared_ptr<JSONRESTHandler>> handlers_;
		std::shared_ptr<JSONRESTHandler> defaultHandler_;
};

RESTServer::RESTServer(std::shared_ptr<boost::asio::io_service> ioService) {
	private_ = std::make_shared<RESTServer::Private>(ioService);
	private_->onWebSocketConnection.connect(onWebSocketConnection);
}

RESTServer::~RESTServer() {

}

bool RESTServer::listen(int port) {
	return private_->start(port);
}

void RESTServer::setReuseAddr(bool b) {
	private_->setReuseAddr(b);
}

void RESTServer::addDefaultGetEndpoint(std::shared_ptr<JSONRESTHandler> handler) {
	private_->addDefaultGetEndpoint(handler);
}

void RESTServer::addJSONEndpoint(const PathVerb& pathVerb, std::shared_ptr<JSONRESTHandler> handler) {
	private_->addJSONEndpoint(pathVerb, handler);
}

void RESTServer::poll() {
	private_->poll();
}

void RESTServer::run() {
	private_->run();
}

}
