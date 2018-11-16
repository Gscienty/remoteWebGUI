#include "web/req.hpp"
#ifdef DEBUG 
#include <fcntl.h>
#include <iostream>
#endif

namespace rwg_web {
req::req()
    : _fd(0)
    , _security(false)
    , _ssl(nullptr)
    , _stat(rwg_web::req_stat::req_stat_header_method)
    , _buf_size(0)
    , _buf(nullptr)
    , _buf_avail_size(0)
    , _raw(nullptr)
    , _raw_len(0)
    , _raw_readed_len(0) {}

req::~req() {
    this->free_buf();
    if (this->_raw != nullptr) {
        delete [] this->_raw;
    }
}

void req::__load() {
    this->_buf_pos = 0;
    ::ssize_t ret = 0;

    if (this->_security) {
#ifdef DEBUG
        std::cout << "req (tls) load data" << std::endl;
#endif
        ret = ::SSL_read(this->_ssl, this->_buf, this->_buf_size);
        if (ret <= 0) {
            this->_buf_avail_size = 0;
            if (ret == 0) {
#ifdef DEBUG
                std::cout << "tls read error (part 1)" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
            else if (SSL_get_error(this->_ssl, ret) != SSL_ERROR_WANT_READ) {
#ifdef DEBUG
                std::cout << "tls read error (part 2)" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
#ifdef DEBUG
            else {
                std::cout << "tls read need next" << std::endl;
            }
#endif
            return;
        }
    }
    else {
#ifdef DEBUG
        std::cout << "req load data" << std::endl;

        auto flag = fcntl(this->_fd, F_GETFL, 0);
        std::cout << this->_fd << ' ' << (flag & O_NONBLOCK) << std::endl;
#endif
        ret = ::read(this->_fd, this->_buf, this->_buf_size);

        if (ret <= 0) {
            this->_buf_avail_size = 0;
            if (ret == 0) {
#ifdef DEBUG
            std::cout << "req load data interrupt" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_interrupt;
            }
            else if (ret != EAGAIN) {
#ifdef DEBUG
                std::cout << "req load data error" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
#ifdef DEBUG
            else {
                std::cout << "req load need next" << std::endl;
            }
#endif
            return;
        }
#ifdef DEBUG
        else {
            std::cout << this->_buf << std::endl;
        }
#endif
    }

    this->_buf_avail_size = ret;
}

void req::__parse_header() {
#ifdef DEBUG
    std::cout << "request parse header" << std::endl;
#endif
    if (this->_stat == rwg_web::req_stat::req_stat_end ||
        this->_stat == rwg_web::req_stat::req_stat_interrupt ||
        this->_stat == rwg_web::req_stat::req_stat_header_end ||
        this->_stat == rwg_web::req_stat::req_stat_err) {
#ifdef DEBUG
        std::cout << "request parsed header end" << std::endl;
#endif
        return;
    }

    std::stringstream sstr;
    while (this->_stat != rwg_web::req_stat::req_stat_end &&
           this->_stat != rwg_web::req_stat::req_stat_interrupt &&
           this->_stat != rwg_web::req_stat::req_stat_header_end &&
           this->_stat != rwg_web::req_stat::req_stat_err) {
        if (this->_buf_pos == this->_buf_avail_size) {
            this->__load();
            if (this->_stat == rwg_web::req_stat::req_stat_interrupt ||
                this->_stat == rwg_web::req_stat::req_stat_err ||
                this->_buf_pos == this->_buf_avail_size) {
                break;
            }
        }

        char c;
        switch (this->_stat) {
        default:
            break;
        case rwg_web::req_stat::req_stat_header_method:
            c = this->_buf[this->_buf_pos++];
            if (c == ' ') {
                /* #ifdef DEBUG */
                /*                     std::cout << "parse completed req [method]" << std::endl; */
                /* #endif */
                this->_stat = rwg_web::req_stat::req_stat_header_uri;
            }
            else { this->_method.push_back(c); }
            break;
        case rwg_web::req_stat::req_stat_header_uri:
            c = this->_buf[this->_buf_pos++];
            if (c == ' ') {
                /* #ifdef DEBUG */
                /*                     std::cout << "parse completed req [uri]" << std::endl; */
                /* #endif */
                this->_stat = rwg_web::req_stat::req_stat_header_verison;
            }
            else { this->_uri.push_back(c); }
            break;
        case rwg_web::req_stat::req_stat_header_verison:
            c = this->_buf[this->_buf_pos++];
            if (c == '\r') {
                /* #ifdef DEBUG */
                /*                     std::cout << "parse completed req [version]" << std::endl; */
                /* #endif */
                this->_stat = rwg_web::req_stat_header_req_end;
            }
            else { this->_version.push_back(c); }
            break;
        case rwg_web::req_stat::req_stat_header_req_end:
            c = this->_buf[this->_buf_pos++];
            if (c == '\n') {
                /* #ifdef DEBUG */
                /*                     std::cout << "parse completed req req header" << std::endl; */
                /* #endif */
                this->_stat = rwg_web::req_stat::req_stat_header_param;
            }
            else {
#ifdef DEBUG
                std::cout << "parse req header error" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
            break;
        case rwg_web::req_stat::req_stat_header_param:
            c = this->_buf[this->_buf_pos++];
            if (c == '\r') {
                /* #ifdef DEBUG */
                /*                     std::cout << "parse req one parameter" << std::endl; */
                /* #endif */
                std::string line = sstr.str();
                sstr.str("");
                std::size_t delimiter = line.find(':');
                std::string key = line.substr(0, delimiter);
                std::string value = line.substr(delimiter + 2);

                this->_header_parameters.insert(std::make_pair(key, value));

                this->_stat = rwg_web::req_stat::req_stat_header_param_end1;
            }
            else {
                sstr.put(c);
            }
            break;
        case rwg_web::req_stat::req_stat_header_param_end1:
            c = this->_buf[this->_buf_pos++];
            if (c == '\n') {
                this->_stat = rwg_web::req_stat::req_stat_header_param_end2;
            }
            else {
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
            break;
        case rwg_web::req_stat::req_stat_header_param_end2:
            c = this->_buf[this->_buf_pos++];
            if (c == '\r') {
                this->_stat = rwg_web::req_stat::req_stat_header_param_end3;
            }
            else {
                /* #ifdef DEBUG */
                /*                     std::cout << "req parse next parameter" << std::endl; */
                /* #endif */
                sstr.put(c);
                this->_stat = rwg_web::req_stat::req_stat_header_param;
            }
            break;
        case rwg_web::req_stat::req_stat_header_param_end3:
            c = this->_buf[this->_buf_pos++];
            if (c == '\n') {
#ifdef DEBUG
                std::cout << "req completed header" << std::endl;
#endif
                this->_stat = rwg_web::req_stat::req_stat_header_end;
            }
            else {
                this->_stat = rwg_web::req_stat::req_stat_err;
            }
            break;
        }
    }
}

void req::__parse_raw() {
    if (this->_raw_len == this->_raw_readed_len) {
        this->_stat = rwg_web::req_stat::req_stat_end;
        return;
    }

    while (this->_raw_readed_len < this->_raw_len) {
        if (this->_buf_pos == this->_buf_avail_size) {
            this->__load();
            if (this->_stat == rwg_web::req_stat::req_stat_interrupt ||
                this->_stat == rwg_web::req_stat::req_stat_err ||
                this->_buf_pos == this->_buf_avail_size) {
                return;
            }
        }
        std::uint64_t readable_len = std::min(this->_buf_avail_size - this->_buf_pos,
                                              this->_raw_len - this->_raw_readed_len);

        std::copy(this->_buf + this->_buf_pos,
                  this->_buf + this->_buf_pos + readable_len,
                  this->_raw + this->_raw_readed_len);

        this->_buf_pos += readable_len;
        this->_raw_readed_len += readable_len;
    }

    this->_stat = rwg_web::req_stat::req_stat_end;
}

void req::use_security(SSL *ssl, bool use) {
    this->_security = use;
#ifdef DEBUG
    std::cout << "req use security [" << this->_security << "]" << std::endl;
#endif
    this->_ssl = ssl;
}

void req::reset() {
    this->_fd = 0;
    this->_stat = rwg_web::req_stat::req_stat_header_method;
    if (this->_raw != nullptr) {
        delete [] this->_raw;
        this->_raw = nullptr;
    }
    this->_raw_len = 0;
    this->_raw_readed_len = 0;
    this->_method.clear();
    this->_uri.clear();
    this->_version.clear();
    this->_header_parameters.clear();
}

void req::alloc_buf(int size) {
    if (this->_buf != nullptr) {
        delete [] this->_buf;
    }

    this->_buf_size = size;
    this->_buf = new char[size];
    this->_buf_avail_size = 0;
    this->_buf_pos = 0;
}

void req::free_buf() {
    if (this->_buf != nullptr) {
        delete [] this->_buf;
    }
    this->_buf_size = 0;
    this->_buf = nullptr;
    this->_buf_avail_size = 0;
    this->_buf_pos = 0;
}

rwg_web::req_stat req::stat() const { return this->_stat; }

void req::parse() {
    if (this->_stat == rwg_web::req_stat::req_stat_raw) {
        this->__parse_raw();
        return;
    }
    this->__parse_header();
#ifdef DEBUG
    std::cout << "receive http req header:" << std::endl;
    std::cout << this->_method << ' ' << this->_uri << ' ' << this->_version << std::endl;

    for (auto kv : this->_header_parameters) {
        std::cout << kv.first << ": " << kv.second << std::endl;
    }
#endif
    if (this->_stat == rwg_web::req_stat::req_stat_header_end) {
        auto len_itr = this->_header_parameters.find("Content-Length");
        if (len_itr != this->_header_parameters.end()) {
            this->_stat = rwg_web::req_stat::req_stat_raw;
            this->_raw_len = std::stoul(len_itr->second);
            this->_raw_readed_len = 0;
            this->__parse_raw();
        }
        else {
            this->_stat = rwg_web::req_stat::req_stat_end;
        }
    }
}

int &req::fd() {
    return this->_fd;
}

std::string &req::method() {
    return this->_method;
}

std::string &req::uri() {
    return this->_uri;
}

std::string &req::version() {
    return this->_version;
}

char *req::raw() const {
    return this->_raw;
}

std::uint64_t req::raw_len() const {
    return this->_raw_len;
}

std::map<std::string, std::string> &req::header_parameters() {
    return this->_header_parameters;
}


}
