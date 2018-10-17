#include "buffer_pool.h"
#include <stdexcept>
#include <algorithm>
#ifdef DEBUG
#include <iostream>
#endif

rwg_http::buffer_pool::buffer_pool(std::size_t pool_size)
    : _pool(new std::uint8_t[pool_size]) {
    
    this->_usable.push_back(std::make_pair(0, pool_size));
}

std::size_t rwg_http::buffer_pool::__calc_min_demand(std::size_t size) const {
    std::size_t ret = 1;
    while (ret < size) {
        ret <<= 1;
    }
    return ret;
}

std::unique_ptr<rwg_http::buffer> rwg_http::buffer_pool::alloc(std::size_t demand_size) {
#ifdef DEBUG
    std::cout << "buffer_pool::alloc: prebegin" << std::endl;
#endif
    std::size_t size = this->__calc_min_demand(demand_size);
    std::unique_lock<std::mutex> lck(this->_pool_mtx);
    this->_usable_cond.wait(lck, [this] () -> bool { return !this->_usable.empty(); });
#ifdef DEBUG
    std::cout << "buffer_pool::alloc: begin" << std::endl;
#endif

    for (auto usable_itr = this->_usable.begin(); usable_itr != this->_usable.end(); usable_itr++) {
        std::size_t block_size = usable_itr->second - usable_itr->first;
        if (block_size >= size) {
            while (block_size != size) {
                block_size >>= 1;
                std::size_t cut_pos = usable_itr->first + block_size;
                this->_usable.insert(usable_itr, std::make_pair(usable_itr->first, cut_pos));
                usable_itr->first = cut_pos;
                usable_itr--;
            }

            auto block = *usable_itr;
            this->_usable.erase(usable_itr);
            auto use_itr = this->__use(block);

#ifdef DEBUG
            std::cout << "buffer_pool::alloc: end" << std::endl;
#endif
            return std::unique_ptr<rwg_http::buffer>(new rwg_http::buffer(std::bind(&rwg_http::buffer_pool::__recover,
                                                                                    this,
                                                                                    std::placeholders::_1),
                                                                          demand_size,
                                                                          &this->_pool.get()[block.first],
                                                                          use_itr));
        }
    }

#ifdef DEBUG
    std::cout << "BAD ALLOC\n";
#endif
    throw std::bad_alloc();
}

std::list<std::pair<std::size_t, std::size_t>>::iterator
rwg_http::buffer_pool::__use(std::pair<std::size_t, std::size_t> block) {
#ifdef DEBUG
    std::cout << "buffer_pool::__use: begin" << std::endl;
#endif
    auto itr = this->_unusable.begin();
    if (itr == this->_unusable.end() || block.second <= itr->first) {
        this->_unusable.push_front(block);
        return this->_unusable.begin();
    }

    while (itr != this->_unusable.end() && block.first >= itr->second) { itr++; }
#ifdef DEBUG
    std::cout << "buffer_pool::__use: end" << std::endl;
#endif
    return this->_unusable.insert(itr, block);
}

void rwg_http::buffer_pool::__recover(std::list<std::pair<std::size_t, std::size_t>>::iterator using_block) {
#ifdef DEBUG
    std::cout << "buffer_pool::__recover: prebegin" << std::endl;
#endif
    auto block = *using_block;
    std::unique_lock<std::mutex> lck(this->_pool_mtx);
#ifdef DEBUG
    std::cout << "buffer_pool::__recover: begin" << std::endl;
#endif
    this->_unusable.erase(using_block);

    auto rc_itr = this->_usable.end();
    auto itr = this->_usable.begin();
    if (itr == this->_usable.end() || block.second <= itr->first) {
        this->_usable.push_front(block);
        rc_itr = this->_usable.begin();
    }
    else {
        while (itr != this->_usable.end() && block.first >= itr->second) { itr++; }
        rc_itr = this->_usable.insert(itr, block);
    }

    bool combined = true;

    while (combined) {
        combined = false;
        auto after = rc_itr;
        after++;
        if (after != this->_usable.end() && after->second - after->first == rc_itr->second - rc_itr->first) {
            rc_itr->second = after->second;
            this->_usable.erase(after);
            combined = true;
            continue;
        }

        if (rc_itr != this->_usable.begin()) {
            auto before = rc_itr;
            before--;
            if (before->second - before->first == rc_itr->second - rc_itr->first) {
                rc_itr->first = before->first;
                this->_usable.erase(before);
                combined = true;
            }
        }
    }

    lck.unlock();
    this->_usable_cond.notify_one();
#ifdef DEBUG
    std::cout << "buffer_pool::__recover: end" << std::endl;
#endif
}


