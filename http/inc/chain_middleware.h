#pragma once

#include "session.h"
#include <list>
#include <functional>

namespace rwg_http {

class chain_middleware {
private:
    std::list<std::function<bool (rwg_http::session&)>> _middlewares;

public:
    void append(std::function<bool (rwg_http::session&)> middleware);

    template<typename _T_Middleware>
    void append(_T_Middleware& middleware) {
        this->_middlewares
            .push_back(std::bind(&_T_Middleware::run, &middleware, std::placeholders::_1));
    }

    void execute(rwg_http::session& session) const;
};

}
