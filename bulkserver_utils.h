#pragma once

#include <iostream>
#include "async_utils.h"

namespace otus_hw10{
    using  std::istream;
    struct Options : public otus_hw9::Options
    {
        using BaseCls_t = otus_hw9::Options;
        uint16_t    port;
        Options() : port(9000) {}
        Options(uint16_t p, size_t cmd_bulk_sz, istream* istrm, size_t thread_cnt) : BaseCls_t(cmd_bulk_sz, istrm, thread_cnt), port(p) {}
        virtual BaseCls_t& add_options(otus_hw7::po::options_description& desc) override;        
    };
};