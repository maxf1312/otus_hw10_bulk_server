#pragma once

#include <iostream>
#include <boost/program_options.hpp>

namespace otus_hw7{
    namespace po = boost::program_options;
    using std::istream;
    using std::ostream;
    struct Options
    {
        bool      show_help;
        size_t    cmd_chunk_sz;
        istream*  is_;
        Options() : show_help(false), cmd_chunk_sz(3), is_(nullptr) {}
        Options(size_t cmd_bulk_sz, istream* istrm = nullptr) : show_help(false), cmd_chunk_sz(cmd_bulk_sz), is_(istrm) {}
        virtual bool parse_command_line(int argc, const char* argv[]);
        virtual Options& add_options(otus_hw7::po::options_description& desc);
    };
};