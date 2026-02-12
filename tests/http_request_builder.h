#pragma once

#include "http/agenui_http_service.h"

namespace agenui {

/**
 * @brief HTTP 请求构建器（测试工具类）
 * 
 * 用于在测试中方便地构建 HttpRequest 对象
 */
class HttpRequestBuilder {
public:
    HttpRequestBuilder() = default;

    HttpRequestBuilder& method(HttpMethod m) {
        m_request.method = m;
        return *this;
    }

    HttpRequestBuilder& url(const std::string& u) {
        m_request.url = u;
        return *this;
    }

    HttpRequestBuilder& header(const std::string& name, const std::string& value) {
        m_request.headers[name] = value;
        return *this;
    }

    HttpRequestBuilder& body(const std::string& b) {
        m_request.body = b;
        return *this;
    }

    HttpRequestBuilder& timeout(int ms) {
        m_request.timeoutMs = ms;
        return *this;
    }

    HttpRequestBuilder& bearerToken(const std::string& token) {
        m_request.headers["Authorization"] = "Bearer " + token;
        return *this;
    }

    HttpRequestBuilder& contentType(const std::string& type) {
        m_request.headers["Content-Type"] = type;
        return *this;
    }

    HttpRequest build() const { return m_request; }

private:
    HttpRequest m_request;
};

}  // namespace agenui
