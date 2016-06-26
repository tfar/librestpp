/*
 * Copyright (c) 2015 Isode Limited.
 * All rights reserved.
 * See the LICENSE file for more information.
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace librestpp {

	class JSONValue {
		public:
			typedef std::shared_ptr<JSONValue> ref;
			virtual ~JSONValue();
	};

	class JSONNull : public JSONValue, public std::enable_shared_from_this<JSONNull> {
	};

	class JSONInt : public JSONValue, public std::enable_shared_from_this<JSONInt> {
		public:
			JSONInt(int value = 0);
			JSONValue::ref set(int value);
			int getValue();
		private:
			int value_;
	};

	class JSONDouble : public JSONValue, public std::enable_shared_from_this<JSONDouble> {
		public:
			JSONDouble(double value = 0);
			JSONValue::ref set(double value);
			double getValue();
		private:
			double value_;
	};

	class JSONString : public JSONValue, public std::enable_shared_from_this<JSONString> {
		public:
			JSONString(const std::string& value = "");
			JSONValue::ref set(const std::string& value);
			std::string getValue();
		private:
			std::string value_;
	};

	class JSONBool : public JSONValue, public std::enable_shared_from_this<JSONBool> {
		public:
			JSONBool(bool value = false);
			JSONValue::ref set(bool value);
			bool getValue();
		private:
			bool value_;
	};

	class JSONArray : public JSONValue, public std::enable_shared_from_this<JSONArray> {
		public:
			JSONArray();
			JSONValue::ref append(JSONValue::ref value);
			std::vector<JSONValue::ref> getValues();
			virtual std::string serialize();
		private:
			std::vector<JSONValue::ref> values_;
	};

	class JSONObject : public JSONValue, public std::enable_shared_from_this<JSONObject> {
		public:
			JSONObject();
			JSONValue::ref set(const std::string& key, JSONValue::ref value);
			JSONValue::ref set(const std::string& key, const std::string& value);
			/**
			 * Parse the provided string, returning a NULL shared_ptr if the JSON
			 * could not be parsed
			 */
			static std::shared_ptr<JSONObject> parse(const std::string& source);
			std::map<std::string, JSONValue::ref> getValues();
			virtual std::string serialize();
		private:
			std::map<std::string, JSONValue::ref> values_;
	};
}
