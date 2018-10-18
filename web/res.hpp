#ifndef _RWG_WEB_RES_
#define _RWG_WEB_RES_

#include <string>
#include <map>
#include <unistd.h>

namespace rwg_web {

static const char __crlf[] = { '\r', '\n' };
static const char __param_delimiter[] = { ':', ' ' };

class res {
private:
    int _fd;
    std::string _version;
    short _status_code;
    std::string _description;
    std::map<std::string, std::string> _header_parameters;

    char * const _buf;
    int _buf_pos;
    const int _buf_size;

    void __putc(char c) {
        if (this->_buf_pos == this->_buf_size) {
            this->flush();
        }
        this->_buf[this->_buf_pos++] = c;
        if (this->_buf_pos == this->_buf_size) {
            this->flush();
        }
    }
public:
    res(int fd, int size)
        : _fd(fd)
        , _buf(new char[size])
        , _buf_pos(0)
        , _buf_size(size) {
    }

    virtual ~res() {
        delete [] this->_buf;
    }

    std::string& version() { return this->_version; }
    short& status_code() { return this->_status_code; }
    std::string& description() { return this->_description; }
    std::map<std::string, std::string>& header_parameters() { return this->_header_parameters; }

    void write_header() {
        for (auto c : this->_version) { this->__putc(c); }
        this->__putc(' ');
        std::string status_code = std::to_string(this->_status_code);
        for (auto c : status_code) { this->__putc(c); }
        this->__putc(' ');
        for (auto c : this->_description) { this->__putc(c); }
        for (auto c : __crlf) { this->__putc(c); }

        for (auto kv : this->_header_parameters) {
            for (auto c : kv.first) { this->__putc(c); }
            for (auto c : __param_delimiter) { this->__putc(c); }
            for (auto c : kv.second) { this->__putc(c); }
            for (auto c : __crlf) { this->__putc(c); }
        }

        for (auto c : __crlf) { this->__putc(c); }
    }

    template<typename _T_Input_Itr> void write(_T_Input_Itr begin, _T_Input_Itr end) {
        for (_T_Input_Itr itr = begin; itr != end; itr++) {
            this->__putc(*itr);
        }
    }

    void flush() { 
        if (this->_buf_pos == 0) {
            return;
        }
        ::write(this->_fd, this->_buf, this->_buf_pos);
        this->_buf_pos = 0;
    }
};

}

#endif
