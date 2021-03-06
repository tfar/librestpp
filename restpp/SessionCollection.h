/*
 * Copyright (c) 2015 Isode Limited.
 * All rights reserved.
 * See the LICENSE file for more information.
 */

#pragma once


#include <map>
#include <memory>
#include <string>

namespace librestpp {
  template <typename T>
	class SessionCollection {
		public:
			virtual ~SessionCollection() {}

			T getSession(const std::string& sessionKey) {
                return sessions_[sessionKey];
            };
        protected:
            std::map<std::string, T> sessions_;
	};
}
